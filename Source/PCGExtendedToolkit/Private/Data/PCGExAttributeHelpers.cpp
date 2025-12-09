// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExAttributeHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "PCGExBroadcast.h"
#include "Data/PCGExDataHelpers.h"
#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "PCGParamData.h"
#include "Data/PCGExDataValue.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendMinMax.h"

bool PCGEx::FAttributeIdentity::InDataDomain() const
{
	return Identifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data;
}

namespace PCGEx
{
#pragma region Attribute utils

	FString FAttributeIdentity::GetDisplayName() const
	{
		return FString(Identifier.Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType));
	}

	bool FAttributeIdentity::operator==(const FAttributeIdentity& Other) const
	{
		return Identifier == Other.Identifier;
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList)
	{
		if (!InMetadata) { return; }

		TArray<FPCGAttributeIdentifier> Identifiers;
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAllAttributes(Identifiers, Types);

		const int32 NumAttributes = Identifiers.Num();

		OutIdentities.Reserve(OutIdentities.Num() + NumAttributes);

		for (int i = 0; i < NumAttributes; i++)
		{
			if (OptionalIgnoreList && OptionalIgnoreList->Contains(Identifiers[i].Name)) { continue; }
			OutIdentities.AddUnique(FAttributeIdentity(Identifiers[i], Types[i], InMetadata->GetConstAttribute(Identifiers[i])->AllowsInterpolation()));
		}
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FPCGAttributeIdentifier>& OutIdentifiers, TMap<FPCGAttributeIdentifier, FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList)
	{
		if (!InMetadata) { return; }

		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAllAttributes(OutIdentifiers, Types);

		const int32 NumAttributes = OutIdentifiers.Num();
		OutIdentities.Reserve(OutIdentities.Num() + NumAttributes);

		for (int i = 0; i < NumAttributes; i++)
		{
			const FPCGAttributeIdentifier& Identifier = OutIdentifiers[i];
			if (OptionalIgnoreList && OptionalIgnoreList->Contains(Identifier.Name)) { continue; }
			OutIdentities.Add(Identifier, FAttributeIdentity(Identifier, Types[i], InMetadata->GetConstAttribute(Identifier)->AllowsInterpolation()));
		}
	}

	bool FAttributeIdentity::Get(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, FAttributeIdentity& OutIdentity)
	{
		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid() || FixedSelector.GetSelection() != EPCGAttributePropertySelection::Attribute) { return false; }

		const FPCGMetadataAttributeBase* Attribute = InData->Metadata->GetConstAttribute(GetAttributeIdentifier(FixedSelector, InData));
		if (!Attribute) { return false; }

		OutIdentity.Identifier = Attribute->Name;
		OutIdentity.UnderlyingType = static_cast<EPCGMetadataTypes>(Attribute->GetTypeId());
		OutIdentity.bAllowsInterpolation = Attribute->AllowsInterpolation();

		return true;
	}

	int32 FAttributeIdentity::ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func)
	{
		// BUG : This does not account for metadata domains

		if (!InMetadata) { return 0; }

		TArray<FPCGAttributeIdentifier> Identifiers;
		TArray<EPCGMetadataTypes> Types;

		InMetadata->GetAllAttributes(Identifiers, Types);
		const int32 NumAttributes = Identifiers.Num();

		for (int i = 0; i < NumAttributes; i++)
		{
			const FAttributeIdentity Identity = FAttributeIdentity(Identifiers[i], Types[i], InMetadata->GetConstAttribute(Identifiers[i])->AllowsInterpolation());
			Func(Identity, i);
		}

		return NumAttributes;
	}

	bool FAttributesInfos::Contains(const FName AttributeName, const EPCGMetadataTypes Type)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Identifier.Name == AttributeName && Identity.UnderlyingType == Type) { return true; } }
		return false;
	}

	bool FAttributesInfos::Contains(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Identifier.Name == AttributeName) { return true; } }
		return false;
	}

	FAttributeIdentity* FAttributesInfos::Find(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Identifier.Name == AttributeName) { return &Identity; } }
		return nullptr;
	}

	bool FAttributesInfos::FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing)
	{
		bool bAnyMissing = false;
		for (const FName& Id : Checklist)
		{
			if (!Contains(Id) || !IsWritableAttributeName(Id))
			{
				OutMissing.Add(Id);
				bAnyMissing = true;
			}
		}
		return bAnyMissing;
	}

	bool FAttributesInfos::FindMissing(const TArray<FName>& Checklist, TSet<FName>& OutMissing)
	{
		bool bAnyMissing = false;
		for (const FName& Id : Checklist)
		{
			if (!Contains(Id) || !IsWritableAttributeName(Id))
			{
				OutMissing.Add(Id);
				bAnyMissing = true;
			}
		}
		return bAnyMissing;
	}

	void FAttributesInfos::Append(const TSharedPtr<FAttributesInfos>& Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch)
	{
		for (int i = 0; i < Other->Identities.Num(); i++)
		{
			const FAttributeIdentity& OtherId = Other->Identities[i];

			if (!InGatherDetails.Test(OtherId.Identifier.Name.ToString())) { continue; }

			if (const int32* Index = Map.Find(OtherId.Identifier))
			{
				const FAttributeIdentity& Id = Identities[*Index];
				if (Id.UnderlyingType != OtherId.UnderlyingType)
				{
					OutTypeMismatch.Add(Id.Identifier.Name);
					// TODO : Update existing based on settings
				}

				continue;
			}

			FPCGMetadataAttributeBase* Attribute = Other->Attributes[i];
			int32 AppendIndex = Identities.Add(OtherId);
			Attributes.Add(Attribute);
			Map.Add(OtherId.Identifier, AppendIndex);
		}
	}

	void FAttributesInfos::Append(const TSharedPtr<FAttributesInfos>& Other, TSet<FName>& OutTypeMismatch, const TSet<FName>* InIgnoredAttributes)
	{
		for (int i = 0; i < Other->Identities.Num(); i++)
		{
			const FAttributeIdentity& OtherId = Other->Identities[i];

			if (InIgnoredAttributes && InIgnoredAttributes->Contains(OtherId.Identifier.Name)) { continue; }

			if (const int32* Index = Map.Find(OtherId.Identifier))
			{
				const FAttributeIdentity& Id = Identities[*Index];
				if (Id.UnderlyingType != OtherId.UnderlyingType)
				{
					OutTypeMismatch.Add(Id.Identifier.Name);
					// TODO : Update existing based on settings
				}

				continue;
			}

			FPCGMetadataAttributeBase* Attribute = Other->Attributes[i];
			int32 AppendIndex = Identities.Add(OtherId);
			Attributes.Add(Attribute);
			Map.Add(OtherId.Identifier, AppendIndex);
		}
	}

	void FAttributesInfos::Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch)
	{
		// TODO : Update types and attributes according to input settings?
	}

	void FAttributesInfos::Filter(const FilterCallback& FilterFn)
	{
		TArray<FName> FilteredOutNames;
		FilteredOutNames.Reserve(Identities.Num());

		for (const TPair<FPCGAttributeIdentifier, int32>& Pair : Map)
		{
			if (FilterFn(Pair.Key.Name)) { continue; }
			FilteredOutNames.Add(Pair.Key.Name);
		}

		// Filter out identities & attributes
		for (FName FilteredOutName : FilteredOutNames)
		{
			Map.Remove(FilteredOutName);
			for (int i = 0; i < Identities.Num(); i++)
			{
				if (Identities[i].Identifier.Name == FilteredOutName)
				{
					Identities.RemoveAt(i);
					Attributes.RemoveAt(i);
					break;
				}
			}
		}

		// Refresh indices
		for (int i = 0; i < Identities.Num(); i++) { Map.Add(Identities[i].Identifier, i); }
	}

	TSharedPtr<FAttributesInfos> FAttributesInfos::Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes)
	{
		PCGEX_MAKE_SHARED(NewInfos, FAttributesInfos)
		FAttributeIdentity::Get(InMetadata, NewInfos->Identities);

		UPCGMetadata* MutableData = const_cast<UPCGMetadata*>(InMetadata);
		for (int i = 0; i < NewInfos->Identities.Num(); i++)
		{
			const FAttributeIdentity& Identity = NewInfos->Identities[i];
			NewInfos->Map.Add(Identity.Identifier, i);
			NewInfos->Attributes.Add(MutableData->GetMutableAttribute(Identity.Identifier));
		}

		return NewInfos;
	}

	TSharedPtr<FAttributesInfos> FAttributesInfos::Get(const TSharedPtr<PCGExData::FPointIOCollection>& InCollection, TSet<FName>& OutTypeMismatch, const TSet<FName>* IgnoredAttributes)
	{
		PCGEX_MAKE_SHARED(NewInfos, FAttributesInfos)
		for (const TSharedPtr<PCGExData::FPointIO>& IO : InCollection->Pairs)
		{
			TSharedPtr<FAttributesInfos> Infos = Get(IO->GetIn()->Metadata, IgnoredAttributes);
			NewInfos->Append(Infos, OutTypeMismatch, IgnoredAttributes);
		}

		return NewInfos;
	}

	void GatherAttributes(const TSharedPtr<FAttributesInfos>& OutInfos, const FPCGContext* InContext, const FName InputLabel, const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		TArray<FPCGTaggedData> InputData = InContext->InputData.GetInputsByPin(InputLabel);
		for (const FPCGTaggedData& TaggedData : InputData)
		{
			if (const UPCGParamData* AsParamData = Cast<UPCGParamData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsParamData->Metadata), InDetails, Mismatches);
			}
			else if (const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsSpatialData->Metadata), InDetails, Mismatches);
			}
		}
	}

	TSharedPtr<FAttributesInfos> GatherAttributes(const FPCGContext* InContext, const FName InputLabel, const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		PCGEX_MAKE_SHARED(OutInfos, FAttributesInfos)
		GatherAttributes(OutInfos, InContext, InputLabel, InDetails, Mismatches);
		return OutInfos;
	}

	FAttributeProcessingInfos::FAttributeProcessingInfos(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector)
	{
		Init(InData, InSelector);
	}

	FAttributeProcessingInfos::FAttributeProcessingInfos(const UPCGData* InData, const FName InAttributeName)
	{
		FPCGAttributePropertyInputSelector ProxySelector = FPCGAttributePropertyInputSelector();
		ProxySelector.Update(InAttributeName.ToString());
		Init(InData, ProxySelector);
	}

	FAttributeProcessingInfos::operator EPCGMetadataTypes() const
	{
		if (Attribute) { return static_cast<EPCGMetadataTypes>(Attribute->GetTypeId()); }
		return EPCGMetadataTypes::Unknown;
	}

	void FAttributeProcessingInfos::Init(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector)
	{
		Selector = InSelector.CopyAndFixLast(InData);
		bIsValid = Selector.IsValid();

		if (!bIsValid) { return; }

		SubSelection = FSubSelection(Selector.GetExtraNames());

		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = nullptr;
			bIsValid = false;

			if (const UPCGSpatialData* AsSpatial = Cast<UPCGSpatialData>(InData))
			{
				Attribute = AsSpatial->Metadata->GetConstAttribute(GetAttributeIdentifier(Selector, InData));
				bIsDataDomain = Attribute ? Attribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data : false;
				bIsValid = Attribute ? true : false;
			}
		}
	}

#pragma endregion

#pragma region Attribute Broadcaster

	const FPCGMetadataAttributeBase* IAttributeBroadcaster::GetAttribute() const { return ProcessingInfos; }

	EPCGMetadataTypes IAttributeBroadcaster::GetMetadataType() const
	{
		return EPCGMetadataTypes::Unknown;
	}

	FName IAttributeBroadcaster::GetName() const { return NAME_None; }

	template <typename T>
	bool TAttributeBroadcaster<T>::ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)

	{
		static_assert(PCGEx::GetMetadataType<T>() != EPCGMetadataTypes::Unknown, "T must be of PCG-friendly type. Custom types are unsupported -- you'll have to static_cast the values.");

		ProcessingInfos = FAttributeProcessingInfos(InData, InSelector);
		if (!ProcessingInfos.bIsValid) { return false; }

		if (ProcessingInfos.bIsDataDomain)
		{
			PCGEx::ExecuteWithRightType(ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
			{
				using T_REAL = decltype(DummyValue);
				DataValue = MakeShared<PCGExData::TDataValue<T_REAL>>(PCGExDataHelpers::ReadDataValue(static_cast<const FPCGMetadataAttribute<T_REAL>*>(ProcessingInfos.Attribute)));

				const FSubSelection& S = ProcessingInfos.SubSelection;
				TypedDataValue = S.bIsValid ? S.Get<T_REAL, T>(DataValue->GetValue<T_REAL>()) : PCGEx::Convert<T_REAL, T>(DataValue->GetValue<T_REAL>());
			});
		}
		else
		{
			InternalAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, ProcessingInfos.Selector);
			ProcessingInfos.bIsValid = InternalAccessor.IsValid();
		}

		return ProcessingInfos.bIsValid;
	}

	template <typename T>
	FName TAttributeBroadcaster<T>::GetName() const { return ProcessingInfos.Selector.GetName(); }

	template <typename T>
	bool TAttributeBroadcaster<T>::IsUsable(int32 NumEntries) { return ProcessingInfos.bIsValid && Values.Num() >= NumEntries; }

	template <typename T>
	bool TAttributeBroadcaster<T>::Prepare(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
	{
		Keys = InPointIO->GetInKeys();
		PCGExMath::TypeMinMax(Min, Max);
		return ApplySelector(InSelector, InPointIO->GetIn());
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::Prepare(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO)
	{
		FPCGAttributePropertyInputSelector InSelector = FPCGAttributePropertyInputSelector();
		InSelector.Update(InName.ToString());
		return Prepare(InSelector, InPointIO);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::Prepare(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO)
	{
		return Prepare(GetSelectorFromIdentifier(InIdentifier), InPointIO);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys)
	{
		if (InKeys) { Keys = InKeys; }
		else if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(InData)) { Keys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(PointData); }
		else if (InData->Metadata) { Keys = MakeShared<FPCGAttributeAccessorKeysEntries>(InData->Metadata); }

		if (!Keys) { return false; }

		PCGExMath::TypeMinMax(Min, Max);
		return ApplySelector(InSelector, InData);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FName& InName, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys)
	{
		FPCGAttributePropertyInputSelector InSelector = FPCGAttributePropertyInputSelector();
		InSelector.Update(InName.ToString());
		return PrepareForSingleFetch(InSelector, InData, InKeys);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FPCGAttributeIdentifier& InIdentifier, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys)
	{
		return PrepareForSingleFetch(GetSelectorFromIdentifier(InIdentifier), InData, InKeys);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FPCGAttributePropertyInputSelector& InSelector, const PCGExData::FTaggedData& InData)
	{
		return PrepareForSingleFetch(InSelector, InData.Data, InData.Keys);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FName& InName, const PCGExData::FTaggedData& InData)
	{
		return PrepareForSingleFetch(InName, InData.Data, InData.Keys);
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::PrepareForSingleFetch(const FPCGAttributeIdentifier& InIdentifier, const PCGExData::FTaggedData& InData)
	{
		return PrepareForSingleFetch(InIdentifier, InData.Data, InData.Keys);
	}

	template <typename T>
	void TAttributeBroadcaster<T>::Fetch(TArray<T>& Dump, const PCGExMT::FScope& Scope)
	{
		check(ProcessingInfos.bIsValid)
		check(Dump.Num() == Keys->GetNum()) // Dump target should be initialized at full length before using Fetch

		if (!ProcessingInfos.bIsValid)
		{
			PCGEX_SCOPE_LOOP(i) { Dump[i] = T{}; }
			return;
		}

		if (DataValue)
		{
			PCGEX_SCOPE_LOOP(i) { Dump[i] = TypedDataValue; }
		}
		else
		{
			TArrayView<T> DumpView = MakeArrayView(Dump.GetData() + Scope.Start, Scope.Count);
			const bool bSuccess = InternalAccessor->GetRange<T>(DumpView, Scope.Start, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);

			if (!bSuccess)
			{
				// TODO : Log error
			}
		}
	}

	template <typename T>
	void TAttributeBroadcaster<T>::GrabAndDump(TArray<T>& Dump, const bool bCaptureMinMax, T& OutMin, T& OutMax)
	{
		const int32 NumPoints = Keys->GetNum();
		PCGEx::InitArray(Dump, NumPoints);

		PCGExMath::TypeMinMax(OutMin, OutMax);

		if (!ProcessingInfos.bIsValid)
		{
			for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
			return;
		}

		if (DataValue)
		{
			for (int i = 0; i < NumPoints; i++) { Dump[i] = TypedDataValue; }

			if (bCaptureMinMax)
			{
				OutMin = TypedDataValue;
				OutMax = TypedDataValue;
			}
		}
		else
		{
			const bool bSuccess = InternalAccessor->GetRange<T>(Dump, 0, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
			if (!bSuccess)
			{
				// TODO : Log error
			}
			else if (bCaptureMinMax)
			{
				for (int i = 0; i < NumPoints; i++)
				{
					const T& V = Dump[i];
					OutMin = PCGExBlend::Min(OutMin, V);
					OutMax = PCGExBlend::Max(OutMax, V);
				}
			}
		}
	}

	template <typename T>
	void TAttributeBroadcaster<T>::GrabUniqueValues(TSet<T>& OutUniqueValues)
	{
		if (!ProcessingInfos.bIsValid) { return; }

		// TODO : Revise this work with a custom has function and output an array of unique values instead

		if constexpr (std::is_same_v<T, FRotator> || std::is_same_v<T, FTransform>)
		{
			UE_LOG(LogTemp, Error, TEXT("Unique value type is unsupported at the moment."))
		}
		else
		{
			if (DataValue)
			{
				OutUniqueValues.Add(TypedDataValue);
			}
			else
			{
				T TempMin = T{};
				T TempMax = T{};

				int32 NumPoints = Keys->GetNum();
				OutUniqueValues.Reserve(OutUniqueValues.Num() + NumPoints);

				TArray<T> Dump;
				GrabAndDump(Dump, false, TempMin, TempMax);
				OutUniqueValues.Append(Dump);

				OutUniqueValues.Shrink();
			}
		}
	}

	template <typename T>
	void TAttributeBroadcaster<T>::Grab(const bool bCaptureMinMax)
	{
		GrabAndDump(Values, bCaptureMinMax, Min, Max);
	}

	template <typename T>
	T TAttributeBroadcaster<T>::FetchSingle(const PCGExData::FElement& Element, const T& Fallback) const
	{
		if (!ProcessingInfos.bIsValid) { return Fallback; }
		if (DataValue) { return TypedDataValue; }

		T OutValue = Fallback;
		if (!InternalAccessor->Get<T>(OutValue, Element.Index, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible)) { OutValue = Fallback; }
		return OutValue;
	}

	template <typename T>
	bool TAttributeBroadcaster<T>::TryFetchSingle(const PCGExData::FElement& Element, T& OutValue) const
	{
		if (!ProcessingInfos.bIsValid) { return false; }
		if (DataValue)
		{
			OutValue = TypedDataValue;
			return true;
		}

		return InternalAccessor->Get<T>(OutValue, Element.Index, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
	}

	template <typename T>
	EPCGMetadataTypes TAttributeBroadcaster<T>::GetMetadataType() const
	{
		return PCGEx::GetMetadataType<T>();
	}

	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		return MakeBroadcaster(FPCGAttributeIdentifier(InName), InPointIO, bSingleFetch);
	}

	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		const FPCGMetadataAttributeBase* Attribute = InPointIO->FindConstAttribute(InIdentifier);
		if (!Attribute) { return nullptr; }

		TSharedPtr<IAttributeBroadcaster> Broadcaster = nullptr;
		ExecuteWithRightType(Attribute->GetTypeId(), [&](auto DummyValue)
		{
			using T = decltype(DummyValue);
			TSharedPtr<TAttributeBroadcaster<T>> TypedBroadcaster = MakeShared<TAttributeBroadcaster<T>>();
			if (!(bSingleFetch ? TypedBroadcaster->PrepareForSingleFetch(InIdentifier, InPointIO->GetIn()) : TypedBroadcaster->Prepare(InIdentifier, InPointIO))) { return; }
			Broadcaster = TypedBroadcaster;
		});

		return Broadcaster;
	}

	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		const UPCGData* InData = InPointIO->GetIn();
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		if (!TryGetType(InSelector, InData, Type)) { return nullptr; }

		TSharedPtr<IAttributeBroadcaster> Broadcaster = nullptr;
		ExecuteWithRightType(Type, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);
			TSharedPtr<TAttributeBroadcaster<T>> TypedBroadcaster = MakeShared<TAttributeBroadcaster<T>>();
			if (!(bSingleFetch ? TypedBroadcaster->PrepareForSingleFetch(InSelector, InData) : TypedBroadcaster->Prepare(InSelector, InPointIO))) { return; }
			Broadcaster = TypedBroadcaster;
		});

		return Broadcaster;
	}

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
		if (!(bSingleFetch ? Broadcaster->PrepareForSingleFetch(InName, InPointIO->GetIn()) : Broadcaster->Prepare(InName, InPointIO))) { return nullptr; }
		return Broadcaster;
	}

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InIdentifier.ToString());
		return MakeTypedBroadcaster<T>(Selector, InPointIO, bSingleFetch);
	}

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch)
	{
		PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
		if (!(bSingleFetch ? Broadcaster->PrepareForSingleFetch(InSelector, InPointIO->GetIn()) : Broadcaster->Prepare(InSelector, InPointIO))) { return nullptr; }
		return Broadcaster;
	}

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXTENDEDTOOLKIT_API TAttributeBroadcaster<_TYPE>;\
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError)
	{
		TSharedPtr<FAttributesInfos> OutInfos = MakeShared<FAttributesInfos>();
		TArray<FPCGTaggedData> TaggedDatas = InContext->InputData.GetInputsByPin(InPinLabel);

		bool bHasErrors = false;

		for (FPCGTaggedData& TaggedData : TaggedDatas)
		{
			const UPCGMetadata* Metadata = nullptr;

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(TaggedData.Data)) { Metadata = ParamData->Metadata; }
			else if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data)) { Metadata = SpatialData->Metadata; }

			if (!Metadata) { continue; }

			TSet<FName> Mismatch;
			TSharedPtr<FAttributesInfos> Infos = FAttributesInfos::Get(Metadata);

			OutInfos->Append(Infos, InGatherDetails, Mismatch);

			if (bThrowError && !Mismatch.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some inputs share the same name but not the same type."));
				bHasErrors = true;
				break;
			}
		}

		if (bHasErrors) { OutInfos.Reset(); }
		return OutInfos;
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"

FPCGExInputConfig::FPCGExInputConfig(const FPCGAttributePropertyInputSelector& InSelector)
{
	Selector.ImportFromOtherSelector(InSelector);
}

FPCGExInputConfig::FPCGExInputConfig(const FPCGExInputConfig& Other)
	: Attribute(Other.Attribute)
{
	Selector.ImportFromOtherSelector(Other.Selector);
}

FPCGExInputConfig::FPCGExInputConfig(const FName InName)
{
	Selector.Update(InName.ToString());
}

#if WITH_EDITOR
FString FPCGExInputConfig::GetDisplayName() const { return GetName().ToString(); }

void FPCGExInputConfig::UpdateUserFacingInfos() { TitlePropertyName = GetDisplayName(); }
#endif

bool FPCGExInputConfig::Validate(const UPCGData* InData)
{
	Selector = Selector.CopyAndFixLast(InData);
	if (GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(PCGEx::GetAttributeIdentifier<true>(Selector, InData)) : nullptr;
		UnderlyingType = Attribute ? Attribute->GetTypeId() : static_cast<int16>(EPCGMetadataTypes::Unknown);
		return Attribute != nullptr;
	}

	if (Selector.IsValid() &&
		Selector.GetSelection() == EPCGAttributePropertySelection::Property)
	{
		UnderlyingType = static_cast<int16>(PCGEx::GetPropertyType(Selector.GetPointProperty()));
		return true;
	}

	return false;
}

bool FPCGExAttributeSourceToTargetDetails::ValidateNames(FPCGExContext* InContext) const
{
	PCGEX_VALIDATE_NAME_CONSUMABLE_C(InContext, Source)
	if (WantsRemappedOutput()) { PCGEX_VALIDATE_NAME_C(InContext, Target) }
	return true;
}

bool FPCGExAttributeSourceToTargetDetails::ValidateNamesOrProperties(FPCGExContext* InContext) const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(Source.ToString());
	if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE_C(InContext, Source)
	}

	if (WantsRemappedOutput())
	{
		Selector.Update(Target.ToString());
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			PCGEX_VALIDATE_NAME_C(InContext, Target)
		}
	}
	return true;
}

FName FPCGExAttributeSourceToTargetDetails::GetOutputName() const
{
	return bOutputToDifferentName ? Target : Source;
}

FPCGAttributePropertyInputSelector FPCGExAttributeSourceToTargetDetails::GetSourceSelector() const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(Source.ToString());
	return Selector;
}

FPCGAttributePropertyInputSelector FPCGExAttributeSourceToTargetDetails::GetTargetSelector() const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(GetOutputName().ToString());
	return Selector;
}

bool FPCGExAttributeSourceToTargetList::ValidateNames(FPCGExContext* InContext) const
{
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Attributes) { if (!Entry.ValidateNames(InContext)) { return false; } }
	return true;
}

void FPCGExAttributeSourceToTargetList::SetOutputTargetNames(const TSharedRef<PCGExData::FFacade>& InFacade) const
{
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Attributes)
	{
		if (!Entry.WantsRemappedOutput()) { continue; }

		const TSharedPtr<PCGExData::IBuffer> Buffer = InFacade->FindWritableAttributeBuffer(Entry.Source);
		if (!Buffer) { continue; }

		Buffer->SetTargetOutputName(Entry.Target);
	}
}

void FPCGExAttributeSourceToTargetList::GetSources(TArray<FName>& OutNames) const
{
	OutNames.Reserve(OutNames.Num() + Attributes.Num());
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Attributes) { OutNames.Add(Entry.Source); }
}

namespace PCGEx
{
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
			OutIdentities.Add(Identifier.Name, FAttributeIdentity(Identifier, Types[i], InMetadata->GetConstAttribute(Identifier)->AllowsInterpolation()));
		}
	}

	bool FAttributeIdentity::Get(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, FAttributeIdentity& OutIdentity)
	{
		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid() || FixedSelector.GetSelection() != EPCGAttributePropertySelection::Attribute) { return false; }

		const FPCGMetadataAttributeBase* Attribute = InData->Metadata->GetConstAttribute(PCGEx::GetAttributeIdentifier<true>(FixedSelector, InData));
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

			if (!InGatherDetails.Test(OtherId.Identifier.ToString())) { continue; }

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
				Attribute = AsSpatial->Metadata->GetConstAttribute(GetAttributeIdentifier<true>(Selector, InData));
				bIsValid = Attribute ? true : false;
			}
		}
	}

	FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector)
	{
		if (InSelector.GetExtraNames().IsEmpty()) { return InSelector.GetName().ToString(); }
		return InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT("."));
	}

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

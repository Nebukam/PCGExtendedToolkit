// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExAttributeHelpers.h"

#include "Data/PCGExData.h"

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

bool FPCGExInputConfig::Validate(const UPCGPointData* InData)
{
	Selector = Selector.CopyAndFixLast(InData);
	if (GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(GetName()) : nullptr;
		UnderlyingType = Attribute ? Attribute->GetTypeId() : static_cast<int16>(EPCGMetadataTypes::Unknown);
		return Attribute != nullptr;
	}

	if (Selector.IsValid() &&
		Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
	{
		UnderlyingType = static_cast<int16>(PCGEx::GetPointPropertyTypeId(Selector.GetPointProperty()));
		return true;
	}

	return false;
}

bool FPCGExAttributeSourceToTargetDetails::ValidateNames(FPCGExContext* InContext) const
{
	PCGEX_VALIDATE_NAME_C(InContext, Source)
	if (bOutputToDifferentName) { PCGEX_VALIDATE_NAME_C(InContext, Target) }
	return true;
}

FName FPCGExAttributeSourceToTargetDetails::GetOutputName() const
{
	return bOutputToDifferentName ? Target : Source;
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
		if (!Entry.bOutputToDifferentName) { continue; }

		const TSharedPtr<PCGExData::FBufferBase> Buffer = InFacade->FindWritableAttributeBuffer(Entry.Source);
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
	bool GetComponentSelection(const TArray<FString>& Names, EPCGExTransformComponent& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExTransformComponent* Selection = STRMAP_TRANSFORM_FIELD.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	bool GetFieldSelection(const TArray<FString>& Names, EPCGExSingleField& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		const FString& STR = Names.Num() > 1 ? Names[1].ToUpper() : Names[0].ToUpper();
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(STR))
		{
			OutSelection = *Selection;
			return true;
		}
		if (STR.Len() <= 0) { return false; }
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(FString::Printf(TEXT("%c"), STR[0]).ToUpper()))
		{
			OutSelection = *Selection;
			return true;
		}
		return false;
	}

	bool GetAxisSelection(const TArray<FString>& Names, EPCGExAxis& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExAxis* Selection = STRMAP_AXIS.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	FPCGAttributePropertyInputSelector CopyAndFixLast(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, TArray<FString>& OutExtraNames)
	{
		//Copy, fix, and clear ExtraNames to support PCGEx custom selectors
		FPCGAttributePropertyInputSelector NewSelector = InSelector.CopyAndFixLast(InData);
		OutExtraNames = NewSelector.GetExtraNames();
		switch (NewSelector.GetSelection())
		{
		default: ;
		case EPCGAttributePropertySelection::Attribute:
			NewSelector.SetAttributeName(NewSelector.GetAttributeName(), true);
			break;
		case EPCGAttributePropertySelection::PointProperty:
			NewSelector.SetPointProperty(NewSelector.GetPointProperty(), true);
			break;
		case EPCGAttributePropertySelection::ExtraProperty:
			NewSelector.SetExtraProperty(NewSelector.GetExtraProperty(), true);
			break;
		}
		return NewSelector;
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities)
	{
		if (!InMetadata) { return; }
		TArray<FName> Names;
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(Names, Types);
		const int32 NumAttributes = Names.Num();
		for (int i = 0; i < NumAttributes; i++)
		{
			OutIdentities.AddUnique(FAttributeIdentity(Names[i], Types[i], InMetadata->GetConstAttribute(Names[i])->AllowsInterpolation()));
		}
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities)
	{
		if (!InMetadata) { return; }
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(OutNames, Types);
		const int32 NumAttributes = OutNames.Num();

		for (int i = 0; i < NumAttributes; i++)
		{
			FName Name = OutNames[i];
			OutIdentities.Add(Name, FAttributeIdentity(Name, Types[i], InMetadata->GetConstAttribute(Name)->AllowsInterpolation()));
		}
	}

	int32 FAttributeIdentity::ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func)
	{
		if (!InMetadata) { return 0; }
		TArray<FName> Names;
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(Names, Types);
		const int32 NumAttributes = Names.Num();
		for (int i = 0; i < NumAttributes; i++)
		{
			const FAttributeIdentity Identity = FAttributeIdentity(Names[i], Types[i], InMetadata->GetConstAttribute(Names[i])->AllowsInterpolation());
			Func(Identity, i);
		}
		return NumAttributes;
	}

	bool FAttributesInfos::Contains(const FName AttributeName, const EPCGMetadataTypes Type)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName && Identity.UnderlyingType == Type) { return true; } }
		return false;
	}

	bool FAttributesInfos::Contains(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName) { return true; } }
		return false;
	}

	FAttributeIdentity* FAttributesInfos::Find(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName) { return &Identity; } }
		return nullptr;
	}

	bool FAttributesInfos::FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing)
	{
		bool bAnyMissing = false;
		for (const FName& Id : Checklist)
		{
			if (!Contains(Id) || !IsValidName(Id))
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
			if (!Contains(Id) || !IsValidName(Id))
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

			if (!InGatherDetails.Test(OtherId.Name.ToString())) { continue; }

			if (const int32* Index = Map.Find(OtherId.Name))
			{
				const FAttributeIdentity& Id = Identities[*Index];
				if (Id.UnderlyingType != OtherId.UnderlyingType)
				{
					OutTypeMismatch.Add(Id.Name);
					// TODO : Update existing based on settings
				}

				continue;
			}

			FPCGMetadataAttributeBase* Attribute = Other->Attributes[i];
			int32 AppendIndex = Identities.Add(OtherId);
			Attributes.Add(Attribute);
			Map.Add(OtherId.Name, AppendIndex);
		}
	}

	void FAttributesInfos::Append(const TSharedPtr<FAttributesInfos>& Other, TSet<FName>& OutTypeMismatch, const TSet<FName>* InIgnoredAttributes)
	{
		for (int i = 0; i < Other->Identities.Num(); i++)
		{
			const FAttributeIdentity& OtherId = Other->Identities[i];

			if (InIgnoredAttributes && InIgnoredAttributes->Contains(OtherId.Name)) { continue; }

			if (const int32* Index = Map.Find(OtherId.Name))
			{
				const FAttributeIdentity& Id = Identities[*Index];
				if (Id.UnderlyingType != OtherId.UnderlyingType)
				{
					OutTypeMismatch.Add(Id.Name);
					// TODO : Update existing based on settings
				}

				continue;
			}

			FPCGMetadataAttributeBase* Attribute = Other->Attributes[i];
			int32 AppendIndex = Identities.Add(OtherId);
			Attributes.Add(Attribute);
			Map.Add(OtherId.Name, AppendIndex);
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

		for (const TPair<FName, int32>& Pair : Map)
		{
			if (FilterFn(Pair.Key)) { continue; }
			FilteredOutNames.Add(Pair.Key);
		}

		// Filter out identities & attributes
		for (FName FilteredOutName : FilteredOutNames)
		{
			Map.Remove(FilteredOutName);
			for (int i = 0; i < Identities.Num(); i++)
			{
				if (Identities[i].Name == FilteredOutName)
				{
					Identities.RemoveAt(i);
					Attributes.RemoveAt(i);
					break;
				}
			}
		}

		// Refresh indices
		for (int i = 0; i < Identities.Num(); i++) { Map.Add(Identities[i].Name, i); }
	}

	TSharedPtr<FAttributesInfos> FAttributesInfos::Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes)
	{
		PCGEX_MAKE_SHARED(NewInfos, FAttributesInfos)
		FAttributeIdentity::Get(InMetadata, NewInfos->Identities);

		UPCGMetadata* MutableData = const_cast<UPCGMetadata*>(InMetadata);
		for (int i = 0; i < NewInfos->Identities.Num(); i++)
		{
			const FAttributeIdentity& Identity = NewInfos->Identities[i];
			NewInfos->Map.Add(Identity.Name, i);
			NewInfos->Attributes.Add(MutableData->GetMutableAttribute(Identity.Name));
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

	void CopyPoints(const PCGExData::FPointIO* Source, const PCGExData::FPointIO* Target, const TSharedPtr<const TArray<int32>>& SourceIndices, const int32 TargetIndex, const bool bKeepSourceMetadataEntry)
	{
		const int32 NumIndices = SourceIndices->Num();
		const TArray<FPCGPoint>& SourceContainer = Source->GetIn()->GetPoints();
		TArray<FPCGPoint>& TargetContainer = Target->GetMutablePoints();

		if (TargetContainer.Num() < TargetIndex + SourceIndices->Num())
		{
			TargetContainer.SetNumUninitialized(TargetContainer.Num() + TargetIndex + SourceIndices->Num());
		}

		if (bKeepSourceMetadataEntry)
		{
			for (int i = 0; i < NumIndices; i++)
			{
				TargetContainer[TargetIndex + i] = SourceContainer[*(SourceIndices->GetData() + i)];
			}
		}
		else
		{
			for (int i = 0; i < NumIndices; i++)
			{
				const int32 WriteIndex = TargetIndex + i;
				const PCGMetadataEntryKey Key = TargetContainer[WriteIndex].MetadataEntry;

				const FPCGPoint& SourcePt = SourceContainer[*(SourceIndices->GetData() + i)];
				FPCGPoint& TargetPt = TargetContainer[WriteIndex] = SourcePt;
				TargetPt.MetadataEntry = Key;
			}
		}
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

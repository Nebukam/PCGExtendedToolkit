// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExAssetCollectionToSet.h"

#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "Collections/PCGExActorCollection.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE AssetCollectionToSet

#pragma region UPCGSettings interface

#if WITH_EDITOR
void UPCGExAssetCollectionToSetSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bWriteAssetClass = bWriteAssetPath;
	AssetClassAttributeName = AssetPathAttributeName;
}
#endif


TArray<FPCGPinProperties> UPCGExAssetCollectionToSetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExAssetCollectionToSetSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("AttributeSet"), TEXT("Attribute set generated from collection"), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExAssetCollectionToSetSettings::CreateElement() const { return MakeShared<FPCGExAssetCollectionToSetElement>(); }

#pragma endregion

#define PCGEX_FOREACH_COL_FIELD(MACRO)\
MACRO(AssetPath, FSoftObjectPath, FSoftObjectPath(), E->Staging.Path)\
MACRO(AssetClass, FSoftClassPath, FSoftClassPath(), E->Staging.Path.ToString())\
MACRO(Weight, int32, 0, E->Weight)\
MACRO(Category, FName, NAME_None, E->Category)\
MACRO(Extents, FVector, FVector::OneVector, E->Staging.Bounds.GetExtent())\
MACRO(BoundsMin, FVector, FVector::OneVector, E->Staging.Bounds.Min)\
MACRO(BoundsMax, FVector, FVector::OneVector, E->Staging.Bounds.Max)\
MACRO(NestingDepth, int32, -1, -1)

bool FPCGExAssetCollectionToSetElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExAssetCollectionToSetSettings* Settings = static_cast<const UPCGExAssetCollectionToSetSettings*>(InSettings);
	PCGEX_GET_OPTION_STATE(Settings->CacheData, bDefaultCacheNodeOutput)
}

bool FPCGExAssetCollectionToSetElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_SETTINGS_C(InContext, AssetCollectionToSet)

	UPCGParamData* OutputSet = NewObject<UPCGParamData>();

	auto OutputToPin = [InContext, OutputSet]()
	{
		FPCGTaggedData& OutData = InContext->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = FName("AttributeSet");
		OutData.Data = OutputSet;

		InContext->Done();
		return InContext->TryComplete();
	};

	PCGExHelpers::LoadBlocking_AnyThread(Settings->AssetCollection.ToSoftObjectPath(), InContext);
	UPCGExAssetCollection* MainCollection = Settings->AssetCollection.Get();

	if (!MainCollection)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Asset collection failed to load."));
		return OutputToPin();
	}

	MainCollection->EDITOR_RegisterTrackingKeys(InContext);

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) bool bOutput##_NAME = Settings->bWrite##_NAME;
	PCGEX_FOREACH_COL_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	// Output actor as FSoftClassPath
	if (Cast<UPCGExActorCollection>(MainCollection)) { bOutputAssetPath = false; }
	else { bOutputAssetClass = false; }

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) \
	FPCGMetadataAttribute<_TYPE>* _NAME##Attribute = nullptr; \
	if(bOutput##_NAME){PCGEX_VALIDATE_NAME(Settings->_NAME##AttributeName) _NAME##Attribute = OutputSet->Metadata->FindOrCreateAttribute<_TYPE>(PCGExMetaHelpers::GetAttributeIdentifier(Settings->_NAME##AttributeName, OutputSet), _DEFAULT, false, true);}
	PCGEX_FOREACH_COL_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	const PCGExAssetCollection::FCache* MainCache = MainCollection->LoadCache();
	TArray<const FPCGExAssetCollectionEntry*> Entries;

	TSet<uint64> GUIDS;

	for (int i = 0; i < MainCache->Main->Order.Num(); i++)
	{
		GUIDS.Empty();
		FPCGExEntryAccessResult Result = MainCollection->GetEntryAt(i);
		ProcessEntry(Result.Entry, Entries, Settings->bOmitInvalidAndEmpty, !Settings->bAllowDuplicates, Settings->SubCollectionHandling, GUIDS);
	}

	if (Entries.IsEmpty()) { return OutputToPin(); }

	for (const FPCGExAssetCollectionEntry* E : Entries)
	{
		const int64 Key = OutputSet->Metadata->AddEntry();

		if (!E || E->bIsSubCollection)
		{
#define PCGEX_SET_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) if(_NAME##Attribute){ _NAME##Attribute->SetValue(Key, _DEFAULT); }
			PCGEX_FOREACH_COL_FIELD(PCGEX_SET_ATT)
#undef PCGEX_SET_ATT
			continue;
		}

#define PCGEX_SET_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) if(_NAME##Attribute){ _NAME##Attribute->SetValue(Key, _VALUE); }
		PCGEX_FOREACH_COL_FIELD(PCGEX_SET_ATT)
#undef PCGEX_SET_ATT
	}

	return OutputToPin();
}

void FPCGExAssetCollectionToSetElement::ProcessEntry(const FPCGExAssetCollectionEntry* InEntry, TArray<const FPCGExAssetCollectionEntry*>& OutEntries, const bool bOmitInvalidAndEmpty, const bool bNoDuplicates, const EPCGExSubCollectionToSet SubHandling, TSet<uint64>& GUIDS)
{
	if (bNoDuplicates) { if (OutEntries.Contains(InEntry)) { return; } }

	auto AddNone = [&]()
	{
		if (bOmitInvalidAndEmpty) { return; }
		OutEntries.Add(nullptr);
	};

	if (!InEntry)
	{
		AddNone();
		return;
	}

	auto AddEmpty = [&](const FPCGExAssetCollectionEntry* S)
	{
		if (bOmitInvalidAndEmpty) { return; }
		OutEntries.Add(S);
	};

	if (InEntry->bIsSubCollection)
	{
		if (SubHandling == EPCGExSubCollectionToSet::Ignore) { return; }

		UPCGExAssetCollection* SubCollection = InEntry->Staging.LoadSync<UPCGExAssetCollection>();
		const PCGExAssetCollection::FCache* SubCache = SubCollection ? SubCollection->LoadCache() : nullptr;

		if (!SubCache)
		{
			AddEmpty(InEntry);
			return;
		}

		bool bVisited = false;
		GUIDS.Add(SubCollection->GetUniqueID(), &bVisited);
		if (bVisited) { return; } // !! Circular dependency !!

		FPCGExEntryAccessResult SubResult;
		switch (SubHandling)
		{
		default: ;
		case EPCGExSubCollectionToSet::Expand: for (int i = 0; i < SubCache->Main->Order.Num(); i++)
			{
				SubResult = SubCollection->GetEntryAt(i);
				ProcessEntry(SubResult.Entry, OutEntries, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
			}
			return;
		case EPCGExSubCollectionToSet::PickRandom: SubResult = SubCollection->GetEntryRandom(0);
			break;
		case EPCGExSubCollectionToSet::PickRandomWeighted: SubResult = SubCollection->GetEntryWeightedRandom(0);
			break;
		case EPCGExSubCollectionToSet::PickFirstItem: SubResult = SubCollection->GetEntryAt(0);
			break;
		case EPCGExSubCollectionToSet::PickLastItem: SubResult = SubCollection->GetEntryAt(SubCache->Main->Indices.Num() - 1);
			break;
		}

		ProcessEntry(SubResult.Entry, OutEntries, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
	}
	else
	{
		OutEntries.Add(InEntry);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
#undef PCGEX_FOREACH_COL_FIELD

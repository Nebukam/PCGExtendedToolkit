﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExAssetCollectionToSet.h"


#include "PCGGraph.h"
#include "PCGPin.h"
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

bool FPCGExAssetCollectionToSetElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(AssetCollectionToSet)

	UPCGParamData* OutputSet = NewObject<UPCGParamData>();

	auto OutputToPin = [Context, OutputSet]()
	{
		FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = FName("AttributeSet");
		OutData.Data = OutputSet;
		return true;
	};

	UPCGExAssetCollection* MainCollection = PCGExHelpers::LoadBlocking_AnyThread(Settings->AssetCollection);


	if (!MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Asset collection failed to load."));
		return OutputToPin();
	}

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) bool bOutput##_NAME = Settings->bWrite##_NAME;
	PCGEX_FOREACH_COL_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	// Output actor as FSoftClassPath
	if (Cast<UPCGExActorCollection>(MainCollection)) { bOutputAssetPath = false; }
	else { bOutputAssetClass = false; }

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) \
	FPCGMetadataAttribute<_TYPE>* _NAME##Attribute = nullptr; \
	if(bOutput##_NAME){PCGEX_VALIDATE_NAME(Settings->_NAME##AttributeName) _NAME##Attribute = OutputSet->Metadata->FindOrCreateAttribute<_TYPE>(PCGEx::GetAttributeIdentifier(Settings->_NAME##AttributeName, OutputSet), _DEFAULT, false, true);}
	PCGEX_FOREACH_COL_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	const PCGExAssetCollection::FCache* MainCache = MainCollection->LoadCache();
	TArray<const FPCGExAssetCollectionEntry*> Entries;

	const FPCGExAssetCollectionEntry* Entry = nullptr;
	const UPCGExAssetCollection* EntryHost = nullptr;

	TSet<uint64> GUIDS;

	for (int i = 0; i < MainCache->Main->Order.Num(); i++)
	{
		GUIDS.Empty();
		MainCollection->GetEntryAt(Entry, i, EntryHost);
		ProcessEntry(Entry, Entries, Settings->bOmitInvalidAndEmpty, !Settings->bAllowDuplicates, Settings->SubCollectionHandling, GUIDS);
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

void FPCGExAssetCollectionToSetElement::ProcessEntry(
	const FPCGExAssetCollectionEntry* InEntry,
	TArray<const FPCGExAssetCollectionEntry*>& OutEntries,
	const bool bOmitInvalidAndEmpty,
	const bool bNoDuplicates,
	const EPCGExSubCollectionToSet SubHandling,
	TSet<uint64>& GUIDS)
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

		const FPCGExAssetCollectionEntry* NestedEntry = nullptr;
		const UPCGExAssetCollection* EntryHost = nullptr;

		switch (SubHandling)
		{
		default: ;
		case EPCGExSubCollectionToSet::Expand:
			for (int i = 0; i < SubCache->Main->Order.Num(); i++)
			{
				SubCollection->GetEntryAt(NestedEntry, i, EntryHost);
				ProcessEntry(NestedEntry, OutEntries, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
			}
			return;
		case EPCGExSubCollectionToSet::PickRandom:
			SubCollection->GetEntryRandom(NestedEntry, 0, EntryHost);
			break;
		case EPCGExSubCollectionToSet::PickRandomWeighted:
			SubCollection->GetEntryWeightedRandom(NestedEntry, 0, EntryHost);
			break;
		case EPCGExSubCollectionToSet::PickFirstItem:
			SubCollection->GetEntryAt(NestedEntry, 0, EntryHost);
			break;
		case EPCGExSubCollectionToSet::PickLastItem:
			SubCollection->GetEntryAt(NestedEntry, SubCache->Main->Indices.Num() - 1, EntryHost);
			break;
		}

		ProcessEntry(NestedEntry, OutEntries, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
	}
	else
	{
		OutEntries.Add(InEntry);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
#undef PCGEX_FOREACH_COL_FIELD

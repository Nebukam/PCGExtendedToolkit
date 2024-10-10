// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExAssetCollectionToSet.h"

#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE AssetCollectionToSet

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExAssetCollectionToSetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExAssetCollectionToSetSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("AttributeSet"), TEXT("Attribute set generated from collection"), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExAssetCollectionToSetSettings::CreateElement() const { return MakeShared<FPCGExAssetCollectionToSetElement>(); }

#pragma endregion

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#define PCGEX_FOREACH_COL_FIELD(MACRO)\
MACRO(AssetPath, FSoftObjectPath, FSoftObjectPath(), S->Path)\
MACRO(Weight, int32, 0, S->Weight)\
MACRO(Category, FName, NAME_None, S->Category)\
MACRO(Extents, FVector, FVector::OneVector, S->Bounds.GetExtent())\
MACRO(BoundsMin, FVector, FVector::OneVector, S->Bounds.Min)\
MACRO(BoundsMax, FVector, FVector::OneVector, S->Bounds.Max)\
MACRO(NestingDepth, int32, -1, -1)
#else
#define PCGEX_FOREACH_COL_FIELD(MACRO)\
MACRO(AssetPath, FString, TEXT(""), S->Path.ToString())\
MACRO(Weight, int32, 0, S->Weight)\
MACRO(Category, FName, NAME_None, S->Category)\
MACRO(Extents, FVector, FVector::OneVector, S->Bounds.GetExtent())\
MACRO(BoundsMin, FVector, FVector::OneVector, S->Bounds.Min)\
MACRO(BoundsMax, FVector, FVector::OneVector, S->Bounds.Max)\
MACRO(NestingDepth, int32, -1, -1)
#endif

FPCGContext* FPCGExAssetCollectionToSetElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

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

	if (!Settings->AssetCollection) { return OutputToPin(); }

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _DEFAULT, _VALUE) \
	const bool bOutput##_NAME = Settings->bWrite##_NAME; \
	FPCGMetadataAttribute<_TYPE>* _NAME##Attribute = nullptr; \
	if(bOutput##_NAME){PCGEX_VALIDATE_NAME(Settings->_NAME##AttributeName) _NAME##Attribute = OutputSet->Metadata->FindOrCreateAttribute<_TYPE>(Settings->_NAME##AttributeName, _DEFAULT, false, true);}
	PCGEX_FOREACH_COL_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	UPCGExAssetCollection* MainCollection = Settings->AssetCollection.LoadSynchronous();

	if (!MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Asset collection failed to load."));
		return OutputToPin();
	}

	const PCGExAssetCollection::FCache* MainCache = MainCollection->LoadCache();
	TArray<const FPCGExAssetStagingData*> StagingDataList;

	const FPCGExAssetStagingData* StagingData = nullptr;

	TSet<uint64> GUIDS;

	for (int i = 0; i < MainCache->Main->Order.Num(); i++)
	{
		GUIDS.Empty();
		MainCollection->GetStagingAt(StagingData, i);
		ProcessStagingData(StagingData, StagingDataList, Settings->bOmitInvalidAndEmpty, !Settings->bAllowDuplicates, Settings->SubCollectionHandling, GUIDS);
	}

	if (StagingDataList.IsEmpty()) { return OutputToPin(); }

	// TODO : Sort StagingDataList according to sorting parameters

	for (const FPCGExAssetStagingData* S : StagingDataList)
	{
		const int64 Key = OutputSet->Metadata->AddEntry();

		if (!S || S->bIsSubCollection)
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

void FPCGExAssetCollectionToSetElement::ProcessStagingData(
	const FPCGExAssetStagingData* InStagingData,
	TArray<const FPCGExAssetStagingData*>& OutStagingDataList,
	const bool bOmitInvalidAndEmpty,
	const bool bNoDuplicates,
	const EPCGExSubCollectionToSet SubHandling,
	TSet<uint64>& GUIDS)
{
	if (bNoDuplicates) { if (OutStagingDataList.Contains(InStagingData)) { return; } }

	auto AddNone = [&]()
	{
		if (bOmitInvalidAndEmpty) { return; }
		OutStagingDataList.Add(nullptr);
	};

	if (!InStagingData)
	{
		AddNone();
		return;
	}

	auto AddEmpty = [&](const FPCGExAssetStagingData* S)
	{
		if (bOmitInvalidAndEmpty) { return; }
		OutStagingDataList.Add(S);
	};

	if (InStagingData->bIsSubCollection)
	{
		if (SubHandling == EPCGExSubCollectionToSet::Ignore) { return; }

		UPCGExAssetCollection* SubCollection = InStagingData->LoadSynchronous<UPCGExAssetCollection>();
		const PCGExAssetCollection::FCache* SubCache = SubCollection ? SubCollection->LoadCache() : nullptr;

		if (!SubCache)
		{
			AddEmpty(InStagingData);
			return;
		}

		bool bVisited = false;
		GUIDS.Add(SubCollection->GetUniqueID(), &bVisited);
		if (bVisited) { return; } // !! Circular dependency !!

		const FPCGExAssetStagingData* NestedStaging = nullptr;

		switch (SubHandling)
		{
		default: ;
		case EPCGExSubCollectionToSet::Expand:
			for (int i = 0; i < SubCache->Main->Order.Num(); i++)
			{
				SubCollection->GetStagingAt(NestedStaging, i);
				ProcessStagingData(NestedStaging, OutStagingDataList, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
			}
			return;
		case EPCGExSubCollectionToSet::PickRandom:
			SubCollection->GetStagingRandom(NestedStaging, 0);
			break;
		case EPCGExSubCollectionToSet::PickRandomWeighted:
			SubCollection->GetStagingWeightedRandom(NestedStaging, 0);
			break;
		case EPCGExSubCollectionToSet::PickFirstItem:
			SubCollection->GetStagingAt(NestedStaging, 0);
			break;
		case EPCGExSubCollectionToSet::PickLastItem:
			SubCollection->GetStagingAt(NestedStaging, SubCache->Main->Indices.Num() - 1);
			break;
		}

		ProcessStagingData(NestedStaging, OutStagingDataList, bOmitInvalidAndEmpty, bNoDuplicates, SubHandling, GUIDS);
	}
	else
	{
		OutStagingDataList.Add(InStagingData);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
#undef PCGEX_FOREACH_COL_FIELD

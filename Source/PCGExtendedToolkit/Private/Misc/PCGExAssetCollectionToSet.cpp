// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAssetCollectionToSet.h"

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

	auto OutputToPin = [&]()
	{
		FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = FName("AttributeSet");
		OutData.Data = OutputSet;
		return true;
	};

	if (!Settings->AssetCollection) { return OutputToPin(); }


	const bool bOutputPath = !Settings->OutputAttributes.AssetPathSourceAttribute.IsNone();
	const bool bOutputWeight = !Settings->OutputAttributes.WeightSourceAttribute.IsNone();
	const bool bOutputCategory = !Settings->OutputAttributes.CategorySourceAttribute.IsNone();

	if (bOutputPath && !PCGEx::IsValidName(Settings->OutputAttributes.AssetPathSourceAttribute))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid AssetPath output attribute name."));
	}

	if (bOutputWeight && !PCGEx::IsValidName(Settings->OutputAttributes.WeightSourceAttribute))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid Weight output attribute name."));
	}

	if (bOutputCategory && !PCGEx::IsValidName(Settings->OutputAttributes.CategorySourceAttribute))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid Category output attribute name."));
	}


	UPCGExAssetCollection* MainCollection = Settings->AssetCollection.LoadSynchronous();

	if (!MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Asset collection failed to load."));
		return OutputToPin();
	}

	PCGExAssetCollection::FCache* MainCache = MainCollection->LoadCache();

	TArray<int32> Weights;
	TArray<FSoftObjectPath> Paths;
	TArray<FName> Categories;

	const FPCGExAssetStagingData* StagingData = nullptr;

	TSet<uint64> GUIDS;

	for (int i = 0; i < MainCache->Order.Num(); i++)
	{
		GUIDS.Empty();
		MainCollection->GetStagingAt(StagingData, i);
		ProcessStagingData(StagingData, Weights, Paths, Categories, Settings->bOmitInvalidAndEmpty, Settings->SubCollectionHandling, GUIDS);
	}

	if (Paths.IsEmpty()) { return OutputToPin(); }


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	FPCGMetadataAttribute<FSoftObjectPath>* PathAttribute = bOutputPath ? OutputSet->Metadata->FindOrCreateAttribute<FSoftObjectPath>(Settings->OutputAttributes.AssetPathSourceAttribute, FSoftObjectPath(), false, true) : nullptr;
#else
	FPCGMetadataAttribute<FString>* PathAttribute = bOutputPath ? OutputSet->Metadata->FindOrCreateAttribute<FString>(Settings->OutputAttributes.AssetPathSourceAttribute, TEXT(""), false, true) : nullptr;
#endif

	FPCGMetadataAttribute<int32>* WeightAttribute = bOutputWeight ? OutputSet->Metadata->FindOrCreateAttribute<int32>(Settings->OutputAttributes.WeightSourceAttribute, 0, false, true) : nullptr;
	FPCGMetadataAttribute<FName>* CategoryAttribute = bOutputCategory ? OutputSet->Metadata->FindOrCreateAttribute<FName>(Settings->OutputAttributes.CategorySourceAttribute, NAME_None, false, true) : nullptr;

	for (int i = 0; i < Paths.Num(); i++)
	{
		int64 Key = OutputSet->Metadata->AddEntry();

		if (PathAttribute)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			PathAttribute->SetValue(Key, Paths[i]);
#else
			PathAttribute->SetValue(i, Paths[i].ToString());
#endif
		}

		if (WeightAttribute) { WeightAttribute->SetValue(Key, Weights[i]); }
		if (CategoryAttribute) { CategoryAttribute->SetValue(Key, Categories[i]); }
	}

	return OutputToPin();
}

void FPCGExAssetCollectionToSetElement::ProcessStagingData(
	const FPCGExAssetStagingData* InStagingData,
	TArray<int32>& Weights,
	TArray<FSoftObjectPath>& Paths,
	TArray<FName>& Categories,
	const bool bOmitInvalidAndEmpty,
	const EPCGExSubCollectionToSet SubHandling,
	TSet<uint64>& GUIDS)
{
	//const FPCGExAssetStagingData* StagingData = nullptr;
	//MainCollection->GetStaging(StagingData, i, i + Settings->Seed);

	auto AddNone = [&]()
	{
		if (bOmitInvalidAndEmpty) { return; }
		Paths.Add(FSoftObjectPath());
		Weights.Add(0);
		Categories.Add(NAME_None);
	};

	if (!InStagingData)
	{
		AddNone();
		return;
	}

	auto AddEmpty = [&](const FPCGExAssetStagingData* S)
	{
		if (bOmitInvalidAndEmpty) { return; }
		Paths.Add(FSoftObjectPath());
		Weights.Add(S->Weight);
		Categories.Add(S->Category);
	};

	if (InStagingData->bIsSubCollection)
	{
		if (SubHandling == EPCGExSubCollectionToSet::Ignore) { return; }

		UPCGExAssetCollection* SubCollection = InStagingData->LoadSynchronous<UPCGExAssetCollection>();
		PCGExAssetCollection::FCache* SubCache = SubCollection ? SubCollection->LoadCache() : nullptr;

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
			for (int i = 0; i < SubCache->Order.Num(); i++)
			{
				SubCollection->GetStagingAt(NestedStaging, i);
				ProcessStagingData(NestedStaging, Weights, Paths, Categories, bOmitInvalidAndEmpty, SubHandling, GUIDS);
			}
			return;
			break;
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
			SubCollection->GetStagingAt(NestedStaging, SubCache->Indices.Num() - 1);
			break;
		}

		ProcessStagingData(NestedStaging, Weights, Paths, Categories, bOmitInvalidAndEmpty, SubHandling, GUIDS);
	}
	else
	{
		Paths.Add(InStagingData->Path);
		Weights.Add(InStagingData->Weight);
		Categories.Add(InStagingData->Category);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

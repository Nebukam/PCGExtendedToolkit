// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExAssetStaging.h"

#define LOCTEXT_NAMESPACE "PCGExAssetStagingElement"
#define PCGEX_NAMESPACE AssetStaging

PCGExData::EInit UPCGExAssetStagingSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(AssetStaging)

TArray<FPCGPinProperties> UPCGExAssetStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExAssetCollection::SourceAssetCollection, "Attribute set to be used as collection.", Required, {})
	}

	return PinProperties;
}

bool FPCGExAssetStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = Settings->AssetCollection.LoadSynchronous();
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
			return false;
		}
	}
	else
	{
		Context->MainCollection = Settings->AttributeSetDetails.TryBuildCollection(Context, PCGExAssetCollection::SourceAssetCollection, false);
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build collection from attribute set."));
			return false;
		}
	}


	PCGEX_VALIDATE_NAME(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
	}

	return true;
}


void FPCGExAssetStagingContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive);
	}
	else
	{
		MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly);
	}
}


void FPCGExAssetStagingElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		// Internal collection, assets have been loaded at this point
		Context->MainCollection->RebuildStagingData(true);
	}
}

bool FPCGExAssetStagingElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	Context->MainCollection->LoadCache();

	return FPCGExPointsProcessorElement::PostBoot(InContext);
}

bool FPCGExAssetStagingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAssetStagingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = Settings->bPruneEmptyPoints;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAssetStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAssetStaging::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Justification = Settings->Justification;
		Justification.Init(ExecutionContext, PointDataFacade);

		Variations = Settings->Variations;
		Variations.Init(Settings->Seed);

		NumPoints = PointDataFacade->GetNum();

		Helper = MakeUnique<PCGExAssetCollection::TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>>(Context->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(ExecutionContext, PointDataFacade)) { return false; }

		bOutputWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::NoOutput;
		bNormalizedWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::Raw;
		bOneMinusWeight = Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInverted || Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInvertedToDensity;

		if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw)
		{
			WeightWriter = PointDataFacade->GetWritable<int32>(Settings->WeightAttributeName, true);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWritable<double>(Settings->WeightAttributeName, true);
		}

#if PCGEX_ENGINE_VERSION > 503
		PathWriter = PointDataFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, true);
#else
		PathWriter = PointDataFacade->GetWritable<FString>(Settings->AssetPathAttributeName, true);
#endif

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		auto InvalidPoint = [&]()
		{
			if (Settings->bPruneEmptyPoints)
			{
				Point.MetadataEntry = -2;
				return;
			}

#if PCGEX_ENGINE_VERSION > 503
			PathWriter->GetMutable(Index) = FSoftObjectPath{};
#else
			PathWriter->GetMutable(Index) = TEXT("");
#endif

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->GetMutable(Index) = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = -1; }
			}
		};

		if (!PointFilterCache[Index])
		{
			InvalidPoint();
			return;
		}

		const FPCGExAssetCollectionEntry* Entry = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, Settings, Context->SourceComponent.Get());

		Helper->GetEntry(Entry, Index, Seed);

		if (!Entry || !Entry->Staging.Bounds.IsValid)
		{
			InvalidPoint();
			return;
		}

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(Entry->Weight) / static_cast<double>(Context->MainCollection->LoadCache()->WeightSum) : Entry->Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->GetMutable(Index) = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = Weight; }
			else { Point.Density = Weight; }
		}

#if PCGEX_ENGINE_VERSION > 503
		PathWriter->GetMutable(Index) = Entry->Staging.Path;
#else
		PathWriter->GetMutable(Index) = Entry->Staging.Path.ToString();
#endif


		if (Variations.bEnabledBefore) { Variations.Apply(Point, Entry->Variations, EPCGExVariationMode::Before); }

		const FBox& StBox = Entry->Staging.Bounds;
		FVector OutScale = Point.Transform.GetScale3D();
		const FBox InBounds = FBox(Point.BoundsMin * OutScale, Point.BoundsMax * OutScale);
		FBox OutBounds = StBox;

		Settings->ScaleToFit.Process(Point, Entry->Staging.Bounds, OutScale, OutBounds);

		Point.BoundsMin = OutBounds.Min;
		Point.BoundsMax = OutBounds.Max;

		FVector OutTranslation = FVector::ZeroVector;
		OutBounds = FBox(OutBounds.Min * OutScale, OutBounds.Max * OutScale);

		Justification.Process(Index, InBounds, OutBounds, OutTranslation);

		Point.Transform.AddToTranslation(Point.Transform.GetRotation().RotateVector(OutTranslation));
		Point.Transform.SetScale3D(OutScale);

		if (Variations.bEnabledAfter) { Variations.Apply(Point, Entry->Variations, EPCGExVariationMode::After); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}

	void FProcessor::Write()
	{
		// TODO : Find a better solution
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		int32 WriteIndex = 0;
		for (int32 i = 0; i < NumPoints; i++) { if (MutablePoints[i].MetadataEntry != -2) { MutablePoints[WriteIndex++] = MutablePoints[i]; } }

		MutablePoints.SetNum(WriteIndex);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

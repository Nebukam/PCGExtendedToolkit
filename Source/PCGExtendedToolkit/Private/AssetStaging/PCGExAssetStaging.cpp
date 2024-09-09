// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExAssetStaging.h"
#include "Collections/PCGExInternalCollection.h"

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

FPCGExAssetStagingContext::~FPCGExAssetStagingContext()
{
	PCGEX_TERMINATE_ASYNC
	UPCGExInternalCollection* InternalCollection = Cast<UPCGExInternalCollection>(MainCollection);
	PCGEX_DELETE_UOBJECT(InternalCollection)
}

void FPCGExAssetStagingContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		MainCollection = Settings->AssetCollection.LoadSynchronous();
		if (MainCollection) { MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly); }
	}
	else
	{
		// Only load assets for internal collections
		// since we need them for staging
		MainCollection = GetDefault<UPCGExInternalCollection>()->GetCollectionFromAttributeSet(
			this, PCGExAssetCollection::SourceAssetCollection,
			Settings->AttributeSetDetails, false);

		if (MainCollection) { MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive); }
	}
}

bool FPCGExAssetStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = Settings->AssetCollection.LoadSynchronous();
	}
	else
	{
		if (Context->MainCollection)
		{
			// Internal collection, assets have been loaded at this point
			Context->MainCollection->RebuildStagingData(true);
		}
	}

	if (!Context->MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
		return false;
	}

	Context->MainCollection->LoadCache(); // Make sure to load the stuff

	PCGEX_VALIDATE_NAME(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
	}

	return true;
}

bool FPCGExAssetStagingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAssetStagingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = Settings->bPruneEmptyPoints;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExAssetStaging
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAssetStaging::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(AssetStaging)

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		Justification = Settings->Justification;
		Justification.Init(Context, PointDataFacade);

		Variations = Settings->Variations;
		Variations.Init(Settings->Seed);

		NumPoints = PointIO->GetNum();

		Helper = new PCGExAssetCollection::FDistributionHelper(LocalTypedContext->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(Context, PointDataFacade)) { return false; }

		bOutputWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::NoOutput;
		bNormalizedWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::Raw;
		bOneMinusWeight = Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInverted || Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInvertedToDensity;

		if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw)
		{
			WeightWriter = PointDataFacade->GetWriter<int32>(Settings->WeightAttributeName, true);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWriter<double>(Settings->WeightAttributeName, true);
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter = PointDataFacade->GetWriter<FSoftObjectPath>(Settings->AssetPathAttributeName, false);
#else
		PathWriter = PointDataFacade->GetWriter<FString>(Settings->AssetPathAttributeName, true);
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
			if (LocalSettings->bPruneEmptyPoints)
			{
				Point.MetadataEntry = -2;
				return;
			}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			PathWriter->Values[Index] = FSoftObjectPath{};
#else
			PathWriter->Values[Index] = TEXT("");
#endif

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->Values[Index] = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = -1; }
			}
		};

		if (!PointFilterCache[Index])
		{
			InvalidPoint();
			return;
		}

		const FPCGExAssetStagingData* StagingData = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		Helper->GetStaging(StagingData, Index, Seed);

		if (!StagingData || !StagingData->Bounds.IsValid)
		{
			InvalidPoint();
			return;
		}

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(StagingData->Weight) / static_cast<double>(LocalTypedContext->MainCollection->LoadCache()->WeightSum) : StagingData->Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->Values[Index] = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = Weight; }
			else { Point.Density = Weight; }
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter->Values[Index] = StagingData->Path;
#else
		PathWriter->Values[Index] = StagingData->Path.ToString();
#endif


		if (Variations.bEnabledBefore) { Variations.Apply(Point, StagingData->Variations, EPCGExVariationMode::Before); }

		const FBox& StBox = StagingData->Bounds;
		FVector OutScale = Point.Transform.GetScale3D();
		const FBox InBounds = FBox(Point.BoundsMin * OutScale, Point.BoundsMax * OutScale);
		FBox OutBounds = StBox;

		LocalSettings->ScaleToFit.Process(Point, StagingData->Bounds, OutScale, OutBounds);

		Point.BoundsMin = OutBounds.Min;
		Point.BoundsMax = OutBounds.Max;

		FVector OutTranslation = FVector::ZeroVector;
		OutBounds = FBox(OutBounds.Min * OutScale, OutBounds.Max * OutScale);

		Justification.Process(Index, InBounds, OutBounds, OutTranslation);

		Point.Transform.AddToTranslation(Point.Transform.GetRotation().RotateVector(OutTranslation));
		Point.Transform.SetScale3D(OutScale);

		if (Variations.bEnabledAfter) { Variations.Apply(Point, StagingData->Variations, EPCGExVariationMode::After); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}

	void FProcessor::Write()
	{
		// TODO : Find a better solution
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		int32 WriteIndex = 0;
		for (int32 i = 0; i < NumPoints; ++i) { if (MutablePoints[i].MetadataEntry != -2) { MutablePoints[WriteIndex++] = MutablePoints[i]; } }

		MutablePoints.SetNum(WriteIndex);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

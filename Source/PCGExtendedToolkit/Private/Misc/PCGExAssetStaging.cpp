// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAssetStaging.h"

#define LOCTEXT_NAMESPACE "PCGExAssetStagingElement"
#define PCGEX_NAMESPACE AssetStaging

PCGExData::EInit UPCGExAssetStagingSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(AssetStaging)

FPCGExAssetStagingContext::~FPCGExAssetStagingContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExAssetStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	Context->MainCollection = Settings->MainCollection.LoadSynchronous();
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

	PCGEX_CONTEXT(AssetStaging)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>* NewBatch)
			{
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

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		Justification = Settings->Justification;
		Justification.Init(Context, PointDataFacade);

		Details = Settings->DistributionSettings;

		NumPoints = PointIO->GetNum();
		MaxIndex = TypedContext->MainCollection->LoadCache()->Indices.Num() - 1;

		PointDataFacade->bSupportsDynamic = true;

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

		if (Details.Distribution == EPCGExDistribution::Index)
		{
			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				IndexGetter = PointDataFacade->GetBroadcaster<int32>(Details.IndexSettings.IndexSource, true);
			}
			else
			{
				IndexGetter = PointDataFacade->GetScopedBroadcaster<int32>(Details.IndexSettings.IndexSource);
			}

			if (!IndexGetter)
			{
				// TODO : Throw
				return false;
			}

			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				MaxInputIndex = static_cast<double>(IndexGetter->Max);
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		// Note : Prototype implementation

		FPCGExAssetStagingData StagingData;
		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Details.SeedComponents, Point,
			Details.LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		bool bValid = false;

		if (Details.Distribution == EPCGExDistribution::WeightedRandom)
		{
			bValid = LocalTypedContext->MainCollection->GetStagingWeightedRandom(StagingData, Seed);
		}
		else if (Details.Distribution == EPCGExDistribution::Random)
		{
			bValid = LocalTypedContext->MainCollection->GetStagingRandom(StagingData, Seed);
		}
		else
		{
			double PickedIndex = IndexGetter->Values[Index];
			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				PickedIndex = PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex);;
				switch (Details.IndexSettings.TruncateRemap)
				{
				case EPCGExTruncateMode::Round:
					PickedIndex = FMath::RoundToInt(PickedIndex);
					break;
				case EPCGExTruncateMode::Ceil:
					PickedIndex = FMath::CeilToDouble(PickedIndex);
					break;
				case EPCGExTruncateMode::Floor:
					PickedIndex = FMath::FloorToDouble(PickedIndex);
					break;
				default:
				case EPCGExTruncateMode::None:
					break;
				}
			}

			bValid = LocalTypedContext->MainCollection->GetStaging(
				StagingData,
				PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
				Seed,
				Details.IndexSettings.PickMode);
		}

		if (!bValid)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			PathWriter->Values[Index] = FSoftObjectPath{};
#else
			PathWriter->Values[Index] = TEXT("");
#endif

			Point.Density = 0;

			if (LocalSettings->bUpdatePointBounds)
			{
				Point.BoundsMin = FVector::ZeroVector;
				Point.BoundsMax = FVector::ZeroVector;
			}

			if (LocalSettings->bUpdatePointScale)
			{
				Point.Transform.SetScale3D(FVector::ZeroVector);
			}

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->Values[Index] = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = -1; }
				else { Point.Density = -1; }
			}

			return;
		}

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(StagingData.Weight) / static_cast<double>(LocalTypedContext->MainCollection->LoadCache()->WeightSum) : StagingData.Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->Values[Index] = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = Weight; }
			else { Point.Density = Weight; }
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter->Values[Index] = StagingData.Path;
#else
		PathWriter->Values[Index] = StagingData.Path.ToString();
#endif


		const FBox& StBox = StagingData.Bounds;
		FVector OutScale = Point.Transform.GetScale3D();
		const FBox InBounds = FBox(Point.BoundsMin * OutScale, Point.BoundsMax * OutScale);
		FBox OutBounds = StBox;

		LocalSettings->ScaleToFit.Process(Point, StagingData.Bounds, OutScale, OutBounds);

		Point.BoundsMin = OutBounds.Min;
		Point.BoundsMax = OutBounds.Max;


		FVector OutTranslation = FVector::ZeroVector;
		OutBounds = FBox(OutBounds.Min * OutScale, OutBounds.Max * OutScale);

		Justification.Process(Index, InBounds, OutBounds, OutTranslation);

		Point.Transform.AddToTranslation(Point.Transform.GetRotation().RotateVector(OutTranslation));
		Point.Transform.SetScale3D(OutScale);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

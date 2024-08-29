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

		if (Settings->Distribution == EPCGExDistribution::Index)
		{
			if (Settings->bRemapIndexToCollectionSize)
			{
				IndexGetter = PointDataFacade->GetBroadcaster<int32>(Settings->IndexSource, true);
			}
			else
			{
				IndexGetter = PointDataFacade->GetScopedBroadcaster<int32>(Settings->IndexSource);
			}

			if (!IndexGetter)
			{
				// TODO : Throw
				return false;
			}

			if (Settings->bRemapIndexToCollectionSize)
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
			LocalSettings->SeedComponents, Point,
			LocalSettings->LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		bool bValid = false;

		if (LocalSettings->Distribution == EPCGExDistribution::WeightedRandom)
		{
			bValid = LocalTypedContext->MainCollection->GetStagingWeightedRandom(StagingData, Seed);
		}
		else if (LocalSettings->Distribution == EPCGExDistribution::Random)
		{
			bValid = LocalTypedContext->MainCollection->GetStagingRandom(StagingData, Seed);
		}
		else
		{
			double PickedIndex = IndexGetter->Values[Index];
			if (LocalSettings->bRemapIndexToCollectionSize)
			{
				PickedIndex = PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex);;
				switch (LocalSettings->TruncateRemap)
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
				PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, LocalSettings->IndexSafety),
				Seed,
				LocalSettings->PickMode);
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

		const FVector OriginalBoundsSize = Point.GetLocalBounds().GetSize();;

		if (LocalSettings->bUpdatePointBounds)
		{
			const FBox Bounds = StagingData.Bounds;
			Point.BoundsMin = Bounds.Min;
			Point.BoundsMax = Bounds.Max;
		}

		if (LocalSettings->bUpdatePointScale)
		{
			if (LocalSettings->bUniformScale)
			{
				const FVector A = OriginalBoundsSize;
				const FVector B = StagingData.Bounds.GetSize();

				const double XFactor = A.X / B.X;
				const double YFactor = A.Y / B.Y;
				const double ZFactor = A.Z / B.Z;

				const double ScaleFactor = FMath::Min3(XFactor, YFactor, ZFactor);
				Point.Transform.SetScale3D(Point.Transform.GetScale3D() * ScaleFactor);
			}
			else
			{
				Point.Transform.SetScale3D(Point.Transform.GetScale3D() * (OriginalBoundsSize / StagingData.Bounds.GetSize()));
			}
		}

		if (LocalSettings->bUpdatePointPivot)
		{
			const FVector Center = Point.GetLocalCenter() * Point.Transform.GetScale3D();
			const FVector Pos = Point.Transform.GetLocation();
			Point.Transform.SetLocation(Pos - Center);
			//Point.SetLocalBounds(FBoxCenterAndExtent(FVector::ZeroVector, Point.GetExtents()).GetBox());
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

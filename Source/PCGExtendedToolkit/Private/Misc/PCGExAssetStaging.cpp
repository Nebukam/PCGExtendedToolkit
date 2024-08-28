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

		PointDataFacade->bSupportsDynamic = true;

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		NumPoints = PointIO->GetNum();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter = PointDataFacade->GetWriter<FSoftObjectPath>(Settings->AssetPathAttributeName, false);
#else
		PathWriter = PointDataFacade->GetWriter<FString>(Settings->AssetPathAttributeName, true);
#endif

		if (Settings->Distribution == EPCGExDistribution::Index)
		{
			IndexCache = PointDataFacade->GetScopedBroadcaster<int32>(Settings->IndexAttribute);
			if (!IndexCache)
			{
				// TODO : Throw
				return false;
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
			bValid = LocalTypedContext->MainCollection->GetStaging(StagingData, Index, Seed);
		}

		if (!bValid)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			PathWriter->Values[Index] = FSoftObjectPath{};
#else
			PathWriter->Values[Index] = TEXT("");
#endif

			Point.Density = 0;

			if (LocalSettings->BoundsStaging == EPCGExBoundsStaging::UpdatePointBounds)
			{
				Point.SetExtents(FVector::ZeroVector);
			}
			else if (LocalSettings->BoundsStaging == EPCGExBoundsStaging::UpdatePointScale)
			{
				Point.Transform.SetScale3D(FVector::ZeroVector);
			}

			return;
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter->Values[Index] = StagingData.Path;
#else
		PathWriter->Values[Index] = StagingData.Path.ToString();
#endif

		if (LocalSettings->BoundsStaging == EPCGExBoundsStaging::Ignore) { return; }

		if (LocalSettings->BoundsStaging == EPCGExBoundsStaging::UpdatePointBounds)
		{
			Point.SetExtents(StagingData.Bounds.GetExtent());
		}
		else if (LocalSettings->BoundsStaging == EPCGExBoundsStaging::UpdatePointScale)
		{
			Point.Transform.SetScale3D(Point.Transform.GetScale3D() * (Point.GetExtents().Length() / StagingData.Bounds.GetExtent().Length()));
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

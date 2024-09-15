// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPickClosestClusters.h"

#include "Data/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExPickClosestClusters"
#define PCGEX_NAMESPACE PickClosestClusters

PCGExData::EInit UPCGExPickClosestClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPickClosestClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPickClosestClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourcePickersLabel, "Target points used to test for proximity", Required, {})
	return PinProperties;
}

void FPCGExPickClosestClustersContext::OnBatchesProcessingDone()
{
	FPCGExEdgesProcessorContext::OnBatchesProcessingDone();

	TArray<PCGExPickClosestClusters::FProcessor*> Processors;
	GatherClusterProcessors(Processors);

	const int32 NumTargets = TargetDataFacade->Source->GetNum();

	PCGEX_SETTINGS_LOCAL(PickClosestClusters)

	if (Settings->PickMode == EPCGExClusterClosestPickMode::OnlyBest)
	{
		for (int i = 0; i < NumTargets; ++i)
		{
			int32 Pick = -1;
			double Closest = TNumericLimits<double>::Max();

			for (int j = 0; j < Processors.Num(); ++j)
			{
				const double Dist = Processors[j]->Distances[i];
				if (Closest > Dist)
				{
					Closest = Dist;
					Pick = j;
				}
			}

			Processors[Pick]->Picker = i;
		}
	}
	else
	{
		for (int i = 0; i < NumTargets; ++i)
		{
			int32 Pick = -1;
			double Closest = TNumericLimits<double>::Max();

			for (int j = 0; j < Processors.Num(); ++j)
			{
				const double Dist = Processors[j]->Distances[i];
				if (Closest > Dist && Processors[j]->Picker == 0)
				{
					Closest = Dist;
					Pick = j;
				}
			}

			Processors[Pick]->Picker = i;
		}
	}
}

PCGEX_INITIALIZE_ELEMENT(PickClosestClusters)

FPCGExPickClosestClustersContext::~FPCGExPickClosestClustersContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE_FACADE_AND_SOURCE(TargetDataFacade)
	PCGEX_DELETE(TargetForwardHandler)
	TargetAttributesToTags.Cleanup();
}


bool FPCGExPickClosestClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PickClosestClusters)

	PCGExData::FPointIO* Targets = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourcePickersLabel, true);
	if (!Targets) { return false; }

	Context->TargetDataFacade = new PCGExData::FFacade(Targets);

	PCGEX_FWD(TargetAttributesToTags)

	if (!Context->TargetAttributesToTags.Init(Context, Context->TargetDataFacade)) { return false; }

	Context->TargetForwardHandler = Settings->TargetForwarding.GetHandler(Context->TargetDataFacade);

	Context->KeepTag = Settings->KeepTag.ToString();
	Context->OmitTag = Settings->OmitTag.ToString();

	return true;
}

bool FPCGExPickClosestClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPickClosestClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PickClosestClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			return true;
		}

		if (!Context->StartProcessingClusters<PCGExPickClosestClusters::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExPickClosestClusters::FProcessorBatch* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputBatches();
	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExPickClosestClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PickClosestClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		const int32 NumTargets = TypedContext->TargetDataFacade->Source->GetNum();
		PCGEX_SET_NUM_UNINITIALIZED(Distances, NumTargets)

		Cluster->RebuildOctree(Settings->SearchMode);

		PCGEX_ASYNC_GROUP(AsyncManager, ProcessTargets)

		if (Settings->SearchMode == EPCGExClusterClosestSearchMode::Edge)
		{
			ProcessTargets->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					Distances[Index] = TNumericLimits<double>::Max();

					const FPCGPoint& Point = LocalTypedContext->TargetDataFacade->Source->GetInPoint(Index);
					const FVector TargetLocation = Point.Transform.GetLocation();

					Cluster->EdgeOctree->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(TargetLocation, Point.GetScaledExtents()), [&](const PCGExCluster::FClusterItemRef& ItemRef)
						{
							Distances[Index] = FMath::Min(Distances[Index], FVector::DistSquared(TargetLocation, Cluster->GetClosestPointOnEdge(ItemRef.ItemIndex, TargetLocation)));
						});
				}, NumTargets, 256);
		}
		else
		{
			ProcessTargets->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					Distances[Index] = TNumericLimits<double>::Max();

					const FPCGPoint& Point = LocalTypedContext->TargetDataFacade->Source->GetInPoint(Index);
					const FVector TargetLocation = Point.Transform.GetLocation();

					Cluster->NodeOctree->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(TargetLocation, Point.GetScaledExtents()), [&](const PCGExCluster::FClusterItemRef& ItemRef)
						{
							Distances[Index] = FMath::Min(Distances[Index], FVector::DistSquared(TargetLocation, Cluster->GetPos(ItemRef.ItemIndex)));
						});
				}, NumTargets, 256);
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (LocalSettings->Action == EPCGExFilterDataAction::Omit) { if (Picker != -1) { return; } }
		else if (LocalSettings->Action == EPCGExFilterDataAction::Keep) { if (Picker == -1) { return; } }
		else
		{
			if (Picker == -1)
			{
				EdgesIO->Tags->Add(LocalTypedContext->OmitTag);
				return;
			}

			EdgesIO->Tags->Add(LocalTypedContext->KeepTag);
		}

		if (!LocalSettings->TargetForwarding.bEnabled)
		{
			EdgesIO->InitializeOutput(PCGExData::EInit::Forward);
			if (!VtxDataFacade->Source->GetOut()) { VtxDataFacade->Source->InitializeOutput(PCGExData::EInit::Forward); }
		}
		else
		{
			EdgesIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			if (!VtxDataFacade->Source->GetOut()) { VtxDataFacade->Source->InitializeOutput(PCGExData::EInit::DuplicateInput); }

			LocalTypedContext->TargetForwardHandler->Forward(Picker, EdgeDataFacade);
			LocalTypedContext->TargetForwardHandler->Forward(Picker, VtxDataFacade);
		}

		LocalTypedContext->TargetAttributesToTags.Tag(Picker, EdgeDataFacade->Source);
		LocalTypedContext->TargetAttributesToTags.Tag(Picker, VtxDataFacade->Source);
	}

	void FProcessorBatch::Output()
	{
		int32 Picks = 0;
		const int32 MaxPicks = Processors.Num();

		for (const FProcessor* P : Processors) { if (P->Picker != -1) { Picks++; } }

		const UPCGExPickClosestClustersSettings* Stg = Processors[0]->LocalSettings;
		const FPCGExPickClosestClustersContext* Ctx = Processors[0]->LocalTypedContext;

		if (Stg->Action == EPCGExFilterDataAction::Omit) { if (Picks == MaxPicks) { return; } }
		else if (Stg->Action == EPCGExFilterDataAction::Keep) { if (Picks == 0) { return; } }

		if (Picks > 0) { VtxIO->Tags->Add(Ctx->KeepTag); }
		else { VtxIO->Tags->Add(Ctx->OmitTag); }

		TBatch<FProcessor>::Output();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPickClosestClusters.h"

#include "Data/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExPickClosestClusters"
#define PCGEX_NAMESPACE PickClosestClusters

PCGExData::EIOInit UPCGExPickClosestClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExPickClosestClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

TArray<FPCGPinProperties> UPCGExPickClosestClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourcePickersLabel, "Target points used to test for proximity", Required, {})
	return PinProperties;
}

void FPCGExPickClosestClustersContext::ClusterProcessing_InitialProcessingDone()
{
	FPCGExEdgesProcessorContext::ClusterProcessing_InitialProcessingDone();

	TArray<TSharedPtr<PCGExPickClosestClusters::FProcessor>> Processors;
	GatherClusterProcessors(Processors);

	const int32 NumTargets = TargetDataFacade->Source->GetNum();

	PCGEX_SETTINGS_LOCAL(PickClosestClusters)

	if (Settings->PickMode == EPCGExClusterClosestPickMode::OnlyBest)
	{
		for (int i = 0; i < NumTargets; i++)
		{
			int32 Pick = -1;
			double Closest = MAX_dbl;

			for (int j = 0; j < Processors.Num(); j++)
			{
				const double Dist = Processors[j]->Distances[i];
				if (Closest > Dist)
				{
					Closest = Dist;
					Pick = j;
				}
			}

			if (Pick == -1) { continue; }
			Processors[Pick]->Picker = i;
		}
	}
	else
	{
		for (int i = 0; i < NumTargets; i++)
		{
			int32 Pick = -1;
			double Closest = MAX_dbl;

			for (int j = 0; j < Processors.Num(); j++)
			{
				const double Dist = Processors[j]->Distances[i];
				if (Closest > Dist && Processors[j]->Picker == 0)
				{
					Closest = Dist;
					Pick = j;
				}
			}

			if (Pick == -1) { continue; }
			Processors[Pick]->Picker = i;
		}
	}
}

PCGEX_INITIALIZE_ELEMENT(PickClosestClusters)

bool FPCGExPickClosestClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PickClosestClusters)

	Context->TargetDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourcePickersLabel, true);
	if (!Context->TargetDataFacade) { return false; }

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
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExPickClosestClusters::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExPickClosestClusters::FBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputBatches();
	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExPickClosestClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }


		Cluster->RebuildOctree(Settings->SearchMode);
		Search();

		return true;
	}

	void FProcessor::Search()
	{
		const int32 NumTargets = Context->TargetDataFacade->Source->GetNum();
		PCGEx::InitArray(Distances, NumTargets);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProcessTargets)

		if (Settings->SearchMode == EPCGExClusterClosestSearchMode::Edge)
		{
			ProcessTargets->OnIterationCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					PCGEX_ASYNC_THIS

					This->Distances[Index] = MAX_dbl;

					const FPCGPoint& Point = This->Context->TargetDataFacade->Source->GetInPoint(Index);
					const FVector TargetLocation = Point.Transform.GetLocation();

					bool bFound = false;
					This->Cluster->GetEdgeOctree()->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(TargetLocation, Point.GetScaledExtents() + FVector(This->Settings->TargetBoundsExpansion)),
						[&](const PCGEx::FIndexedItem& Item)
						{
							This->Distances[Index] = FMath::Min(This->Distances[Index], FVector::DistSquared(TargetLocation, This->Cluster->GetClosestPointOnEdge(Item.Index, TargetLocation)));
							bFound = true;
						});

					if (!bFound && This->Settings->bExpandSearchOutsideTargetBounds)
					{
						This->Cluster->GetEdgeOctree()->FindNearbyElements(
							TargetLocation, [&](const PCGEx::FIndexedItem& Item)
							{
								This->Distances[Index] = FMath::Min(This->Distances[Index], FVector::DistSquared(TargetLocation, This->Cluster->GetPos(Item.Index)));
								bFound = true;
							});
					}
				};
		}
		else
		{
			ProcessTargets->OnIterationCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					PCGEX_ASYNC_THIS

					This->Distances[Index] = MAX_dbl;

					const FPCGPoint& Point = This->Context->TargetDataFacade->Source->GetInPoint(Index);
					const FVector TargetLocation = Point.Transform.GetLocation();

					bool bFound = false;
					This->Cluster->NodeOctree->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(TargetLocation, Point.GetScaledExtents() + FVector(This->Settings->TargetBoundsExpansion)),
						[&](const PCGEx::FIndexedItem& Item)
						{
							This->Distances[Index] = FMath::Min(This->Distances[Index], FVector::DistSquared(TargetLocation, This->Cluster->GetPos(Item.Index)));
							bFound = true;
						});

					if (!bFound && This->Settings->bExpandSearchOutsideTargetBounds)
					{
						This->Cluster->NodeOctree->FindNearbyElements(
							TargetLocation, [&](const PCGEx::FIndexedItem& Item)
							{
								This->Distances[Index] = FMath::Min(This->Distances[Index], FVector::DistSquared(TargetLocation, This->Cluster->GetPos(Item.Index)));
								bFound = true;
							});
					}
				};
		}

		ProcessTargets->StartIterations(NumTargets, 256);
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Action == EPCGExFilterDataAction::Omit) { if (Picker != -1) { return; } }
		else if (Settings->Action == EPCGExFilterDataAction::Keep) { if (Picker == -1) { return; } }
		else
		{
			if (Picker == -1)
			{
				EdgeDataFacade->Source->Tags->Add(Context->OmitTag);
				return;
			}

			EdgeDataFacade->Source->Tags->Add(Context->KeepTag);
		}

		if (!Settings->TargetForwarding.bEnabled)
		{
			EdgeDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
			if (!VtxDataFacade->Source->GetOut()) { VtxDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }
		}
		else
		{
			EdgeDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Duplicate);
			if (!VtxDataFacade->Source->GetOut()) { VtxDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Duplicate); }

			Context->TargetForwardHandler->Forward(Picker, EdgeDataFacade);
			Context->TargetForwardHandler->Forward(Picker, VtxDataFacade);
		}

		Context->TargetAttributesToTags.Tag(Picker, EdgeDataFacade->Source);
		Context->TargetAttributesToTags.Tag(Picker, VtxDataFacade->Source);
	}

	void FBatch::Output()
	{
		int32 Picks = 0;
		const int32 MaxPicks = Processors.Num();

		for (const TSharedRef<FProcessor>& P : Processors) { if (P->Picker != -1) { Picks++; } }

		const UPCGExPickClosestClustersSettings* Stg = Processors[0]->Settings;
		const FPCGExPickClosestClustersContext* Ctx = Processors[0]->Context;

		if (Stg->Action == EPCGExFilterDataAction::Omit) { if (Picks == MaxPicks) { return; } }
		else if (Stg->Action == EPCGExFilterDataAction::Keep) { if (Picks == 0) { return; } }

		if (Picks > 0) { VtxDataFacade->Source->Tags->Add(Ctx->KeepTag); }
		else { VtxDataFacade->Source->Tags->Add(Ctx->OmitTag); }

		TBatch<FProcessor>::Output();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

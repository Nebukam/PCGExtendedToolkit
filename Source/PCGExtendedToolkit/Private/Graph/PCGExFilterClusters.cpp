// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFilterClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExFilterClusters"
#define PCGEX_NAMESPACE FilterClusters

UPCGExFilterClustersSettings::UPCGExFilterClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExFilterClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFilterClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExFilterClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGEx::SourceTargetsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Target points used to test for proximity");
#endif

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(FilterClusters)


FPCGExFilterClustersContext::~FPCGExFilterClustersContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(TargetsCollection)

	for (const TPair<int32, TSet<int32>*> Pair : VtxEdgeMap)
	{
		Pair.Value->Empty();
		delete Pair.Value;
	}

	VtxEdgeMap.Empty();

	Selectors.Empty();
}


bool FPCGExFilterClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FilterClusters)

	Context->TargetsCollection = new PCGExData::FPointIOCollection(Context, PCGEx::SourceTargetsLabel);
	if (Context->TargetsCollection->Pairs.IsEmpty()) { return false; }

	Context->Targets = Context->TargetsCollection->Pairs[0];
	const TArray<FPCGPoint>& InTargetPoints = Context->Targets->GetIn()->GetPoints();
	Context->Selectors.SetNumUninitialized(InTargetPoints.Num());

	for (int i = 0; i < InTargetPoints.Num(); i++)
	{
		Context->Selectors[i] = PCGExFilterCluster::FSelector(InTargetPoints[i].Transform.GetLocation());
	}

	return true;
}

bool FPCGExFilterClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFilterClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FilterClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				continue;
			}

			Context->CurrentEdgeMap = new TSet<int32>();
			Context->VtxEdgeMap.Add(Context->CurrentIO->IOIndex, Context->CurrentEdgeMap);

			while (Context->AdvanceEdges(true))
			{
				if (!Context->CurrentCluster)
				{
					PCGEX_INVALID_CLUSTER_LOG
					continue;
				}

				Context->CurrentCluster->RebuildOctree(Settings->SearchMode);
				Context->CurrentEdgeMap->Add(Context->CurrentEdges->IOIndex);

				for (PCGExFilterCluster::FSelector& Selector : Context->Selectors)
				{
					const int32 ClosestNodeIndex = Context->CurrentCluster->FindClosestNode(Selector.Position, Settings->SearchMode);
					if (ClosestNodeIndex == -1) { continue; }

					PCGExCluster::FNode& ClosestNode = Context->CurrentCluster->Nodes[ClosestNodeIndex];

					const double Dist = FVector::DistSquared(ClosestNode.Position, Selector.Position);
					if (Dist > Selector.ClosestDistance) { continue; }

					Selector.ClosestDistance = Dist;
					Selector.VtxIndex = Context->CurrentIO->IOIndex;
					Selector.EdgesIndex = Context->CurrentEdges->IOIndex;
				}
			}
		}

		Context->Done();
	}

	if (Context->IsDone())
	{
		TSet<int32> SelectedVtx;
		TSet<int32> SelectedEdges;

		for (PCGExFilterCluster::FSelector& Selector : Context->Selectors)
		{
			SelectedVtx.Add(Selector.VtxIndex);
			SelectedEdges.Add(Selector.EdgesIndex);
		}

		if (Settings->FilterMode == EPCGExClusterFilterMode::Keep)
		{
			for (PCGExData::FPointIO* VtxIO : Context->MainPoints->Pairs)
			{
				if (!SelectedVtx.Contains(VtxIO->IOIndex))
				{
					// Ensure all edges are omitted first
					TSet<int32>** BoundEdgesPtr = Context->VtxEdgeMap.Find(VtxIO->IOIndex);
					bool bKeep = false;

					if (BoundEdgesPtr)
					{
						TSet<int32>& BoundEdges = **BoundEdgesPtr;
						for (const int32 E : BoundEdges)
						{
							if (SelectedEdges.Contains(E))
							{
								bKeep = true;
								break;
							}
						}
					}

					if (!bKeep) { continue; }
				}

				VtxIO->InitializeOutput(PCGExData::EInit::Forward);
			}

			for (PCGExData::FPointIO* EdgesIO : Context->MainEdges->Pairs)
			{
				if (!SelectedEdges.Contains(EdgesIO->IOIndex)) { continue; }
				EdgesIO->InitializeOutput(PCGExData::EInit::Forward);
			}
		}
		else
		{
			for (PCGExData::FPointIO* VtxIO : Context->MainPoints->Pairs)
			{
				if (SelectedVtx.Contains(VtxIO->IOIndex))
				{
					// Ensure at least some edges are kept first
					TSet<int32>& BoundEdges = **Context->VtxEdgeMap.Find(VtxIO->IOIndex);
					int32 Num = 0;
					for (const int32 E : BoundEdges) { if (!SelectedEdges.Contains(E)) { Num++; } }

					if (Num == BoundEdges.Num()) { continue; } // No edge is being kept
				}
				VtxIO->InitializeOutput(PCGExData::EInit::Forward);
			}

			for (PCGExData::FPointIO* EdgesIO : Context->MainEdges->Pairs)
			{
				if (SelectedEdges.Contains(EdgesIO->IOIndex)) { continue; }
				EdgesIO->InitializeOutput(PCGExData::EInit::Forward);
			}
		}

		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

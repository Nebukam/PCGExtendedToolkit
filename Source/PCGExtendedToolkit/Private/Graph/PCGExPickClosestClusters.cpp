// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPickClosestClusters.h"

#include "Data/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExPickClosestClusters"
#define PCGEX_NAMESPACE PickClosestClusters

PCGExData::EInit UPCGExPickClosestClustersSettings::GetEdgeOutputInitMode() const { return FilterActions.Action == EPCGExFilterDataAction::Tag ? PCGExData::EInit::Forward : PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPickClosestClustersSettings::GetMainOutputInitMode() const { return FilterActions.Action == EPCGExFilterDataAction::Tag ? PCGExData::EInit::Forward : PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPickClosestClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourcePickersLabel, "Target points used to test for proximity", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PickClosestClusters)

FPCGExPickClosestClustersContext::~FPCGExPickClosestClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	for (const TPair<int32, TSet<int32>*>& Pair : VtxEdgeMap)
	{
		Pair.Value->Empty();
		delete Pair.Value;
	}

	VtxEdgeMap.Empty();
	Selectors.Empty();

	PCGEX_DELETE(Targets)
}


bool FPCGExPickClosestClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PickClosestClusters)

	Context->Targets = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourcePickersLabel, true);
	if (!Context->Targets) { return false; }

	const TArray<FPCGPoint>& InTargetPoints = Context->Targets->GetIn()->GetPoints();
	Context->Selectors.SetNumUninitialized(InTargetPoints.Num());

	for (int i = 0; i < InTargetPoints.Num(); ++i)
	{
		Context->Selectors[i] = PCGExFilterCluster::FPicker(InTargetPoints[i].Transform.GetLocation());
	}

	return true;
}

bool FPCGExPickClosestClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPickClosestClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PickClosestClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges) { continue; }

			Context->CurrentEdgeMap = new TSet<int32>();
			Context->VtxEdgeMap.Add(Context->CurrentIO->IOIndex, Context->CurrentEdgeMap);

			while (Context->AdvanceEdges(true))
			{
				if (!Context->CurrentCluster) { continue; }

				Context->CurrentCluster->RebuildOctree(Settings->SearchMode);
				Context->CurrentEdgeMap->Add(Context->CurrentEdges->IOIndex);

				for (PCGExFilterCluster::FPicker& Pickers : Context->Selectors)
				{
					const int32 ClosestNodeIndex = Context->CurrentCluster->FindClosestNode(Pickers.Position, Settings->SearchMode);
					if (ClosestNodeIndex == -1) { continue; }

					PCGExCluster::FNode& ClosestNode = (*Context->CurrentCluster->Nodes)[ClosestNodeIndex];

					const double Dist = FVector::DistSquared(Context->CurrentCluster->GetPos(ClosestNode), Pickers.Position);
					if (Dist > Pickers.ClosestDistance) { continue; }

					Pickers.ClosestDistance = Dist;
					Pickers.VtxIndex = Context->CurrentIO->IOIndex;
					Pickers.EdgesIndex = Context->CurrentEdges->IOIndex;
				}
			}
		}

		Context->Done();
	}

	if (Context->IsDone())
	{
		TSet<int32> SelectedVtx;
		TSet<int32> SelectedEdges;

		for (PCGExFilterCluster::FPicker& Selector : Context->Selectors)
		{
			SelectedVtx.Add(Selector.VtxIndex);
			SelectedEdges.Add(Selector.EdgesIndex);
		}

		TSet<PCGExData::FPointIO*> KeepList;
		TSet<PCGExData::FPointIO*> OmitList;

		if (Settings->FilterActions.Action == EPCGExFilterDataAction::Omit)
		{
			for (PCGExData::FPointIO* VtxIO : Context->MainPoints->Pairs)
			{
				if (SelectedVtx.Contains(VtxIO->IOIndex))
				{
					// Ensure at least some edges are kept first
					TSet<int32>& BoundEdges = **Context->VtxEdgeMap.Find(VtxIO->IOIndex);
					int32 Num = 0;
					for (const int32 E : BoundEdges) { if (!SelectedEdges.Contains(E)) { Num++; } }

					if (Num == BoundEdges.Num())
					{
						OmitList.Add(VtxIO);
						continue;
					} // No edge is being kept
				}
				KeepList.Add(VtxIO);
			}

			for (PCGExData::FPointIO* EdgesIO : Context->MainEdges->Pairs)
			{
				if (SelectedEdges.Contains(EdgesIO->IOIndex)) { OmitList.Add(EdgesIO); }
				else { KeepList.Add(EdgesIO); }
			}
		}
		else
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

					if (!bKeep)
					{
						OmitList.Add(VtxIO);
						continue;
					}
				}

				KeepList.Add(VtxIO);
			}

			for (PCGExData::FPointIO* EdgesIO : Context->MainEdges->Pairs)
			{
				if (!SelectedEdges.Contains(EdgesIO->IOIndex)) { OmitList.Add(EdgesIO); }
				else { KeepList.Add(EdgesIO); }
			}
		}

		if (Settings->FilterActions.Action == EPCGExFilterDataAction::Tag)
		{
			for (PCGExData::FPointIO* IO : KeepList) { IO->Tags->Add(Settings->FilterActions.KeepTag.ToString()); }
			for (PCGExData::FPointIO* IO : OmitList) { IO->Tags->Add(Settings->FilterActions.OmitTag.ToString()); }
		}
		else
		{
			for (PCGExData::FPointIO* IO : KeepList) { IO->InitializeOutput(PCGExData::EInit::Forward); }
		}

		Context->OutputPointsAndEdges();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

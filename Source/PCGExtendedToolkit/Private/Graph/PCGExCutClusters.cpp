// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCutClusters.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCutEdges"
#define PCGEX_NAMESPACE CutEdges

TArray<FPCGPinProperties> UPCGExCutEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGExGraph::SourcePathsLabel, "Cutting paths.", Required, {})
	if (Mode != EPCGExCutEdgesMode::Edges) { PCGEX_PIN_PARAMS(PCGExCutEdges::SourceNodeFilters, "Node preservation filters.", Normal, {}) }
	if (Mode != EPCGExCutEdgesMode::Nodes) { PCGEX_PIN_PARAMS(PCGExCutEdges::SourceEdgeFilters, "Edge preservation filters.", Normal, {}) }

	return PinProperties;
}

PCGExData::EIOInit UPCGExCutEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }
PCGExData::EIOInit UPCGExCutEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(CutEdges)

bool FPCGExCutEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CutEdges)

	PCGEX_FWD(IntersectionDetails)
	Context->IntersectionDetails.Init();

	PCGEX_FWD(GraphBuilderDetails)
	Context->DistanceDetails = PCGExDetails::MakeDistances(Settings->NodeDistanceSettings, Settings->NodeDistanceSettings);

	if (Settings->Mode != EPCGExCutEdgesMode::Nodes)
	{
		GetInputFactories(Context, PCGExCutEdges::SourceEdgeFilters, Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}

	if (Settings->Mode != EPCGExCutEdgesMode::Edges)
	{
		GetInputFactories(Context, PCGExCutEdges::SourceNodeFilters, Context->NodeFilterFactories, PCGExFactories::ClusterNodeFilters, false);
	}


	TSharedPtr<PCGExData::FPointIOCollection> PathCollection = MakeShared<PCGExData::FPointIOCollection>(Context, PCGExGraph::SourcePathsLabel);
	if (PathCollection->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Empty paths."));
		return false;
	}

	Context->PathFacades.Reserve(PathCollection->Num());
	Context->Paths.Reserve(PathCollection->Num());

	int32 ExcludedNum = 0;

	for (const TSharedPtr<PCGExData::FPointIO>& PathIO : PathCollection->Pairs)
	{
		if (PathIO->GetNum() < 2)
		{
			ExcludedNum++;
			continue;
		}

		TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());
		Facade->bSupportsScopedGet = Context->bScopedAttributeGet;

		Context->PathFacades.Add(Facade.ToSharedRef());
	}

	if (ExcludedNum != 0)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input paths had less than 2 points and will be ignored."));
	}

	if (Context->PathFacades.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No valid paths found."));
		return false;
	}

	PCGEX_FWD(ClosedLoop)
	Context->ClosedLoop.Init();


	return true;
}

bool FPCGExCutEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCutEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CutEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGExPaths::State_BuildingPaths);
		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), BuildPathsTask)

		BuildPathsTask->OnSubLoopStartCallback =
			[Context](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedRef<PCGExData::FFacade> PathFacade = Context->PathFacades[StartIndex];
				const TSharedPtr<PCGExPaths::FPath> Path = PCGExPaths::MakePath(
					PathFacade->Source->GetIn()->GetPoints(), 0,
					Context->ClosedLoop.IsClosedLoop(PathFacade->Source));

				Path->BuildEdgeOctree();

				Context->Paths.Add(Path.ToSharedRef());
			};

		BuildPathsTask->StartSubLoops(Context->PathFacades.Num(), 1);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExPaths::State_BuildingPaths)
	{
		if (!Context->StartProcessingClusters<PCGExCutEdges::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExCutEdges::FBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraph::State_ReadyToCompile)
	if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExCutEdges
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			Settings->Mode != EPCGExCutEdgesMode::Edges,
			Settings->Mode != EPCGExCutEdgesMode::Nodes,
			false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCutEdges::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		EdgeFilterCache.Init(false, EdgeDataFacade->Source->GetNum());
		NodeFilterCache.Init(false, Cluster->Nodes->Num());

		if (Settings->bInvert)
		{
			if (Settings->Mode != EPCGExCutEdgesMode::Nodes) { for (PCGExGraph::FEdge& E : *Cluster->Edges) { E.bValid = false; } }
			if (Settings->Mode != EPCGExCutEdgesMode::Edges) { for (PCGExCluster::FNode& N : *Cluster->Nodes) { N.bValid = false; } }
		}

		if (Settings->Mode != EPCGExCutEdgesMode::Nodes)
		{
			if (!Context->EdgeFilterFactories.IsEmpty())
			{
				EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
				EdgeFilterManager->bUseEdgeAsPrimary = true;
				if (!EdgeFilterManager->Init(ExecutionContext, Context->EdgeFilterFactories)) { return false; }
			}

			StartParallelLoopForEdges();
		}

		if (Settings->Mode != EPCGExCutEdgesMode::Edges)
		{
			if (!Context->NodeFilterFactories.IsEmpty())
			{
				NodeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
				if (!NodeFilterManager->Init(ExecutionContext, Context->NodeFilterFactories)) { return false; }
			}

			StartParallelLoopForNodes();
		}

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
	{
		const int32 MaxIndex = StartIndex + Count;

		EdgeDataFacade->Fetch(StartIndex, Count);

		TArray<PCGExGraph::FEdge>& Edges = *Cluster->Edges;
		if (EdgeFilterManager) { for (int i = StartIndex; i < MaxIndex; i++) { EdgeFilterCache[i] = EdgeFilterManager->Test(Edges[i]); } }
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		if (EdgeFilterCache[EdgeIndex])
		{
			if (Settings->bInvert) { Edge.bValid = true; }
			return;
		}

		const FVector A1 = VtxDataFacade->Source->GetInPoint(Edge.Start).Transform.GetLocation();
		const FVector B1 = VtxDataFacade->Source->GetInPoint(Edge.End).Transform.GetLocation();
		const FVector Dir = (B1 - A1).GetSafeNormal();

		FBox EdgeBox = FBox(ForceInit);
		EdgeBox += A1;
		EdgeBox += B1;

		for (const TSharedRef<PCGExPaths::FPath>& Path : Context->Paths)
		{
			if (!Path->Bounds.Intersect(EdgeBox)) { continue; }

			// Check paths
			Path->GetEdgeOctree()->FindFirstElementWithBoundsTest(
				EdgeBox, [&](const PCGExPaths::FPathEdge* PathEdge)
				{
					//if (Settings->bInvert) { if (Edge.bValid) { return false; } }
					//else if (!Edge.bValid) { return false; }

					if (Context->IntersectionDetails.bUseMinAngle || Context->IntersectionDetails.bUseMaxAngle)
					{
						if (!Context->IntersectionDetails.CheckDot(FMath::Abs(FVector::DotProduct(PathEdge->Dir, Dir))))
						{
							return true;
						}
					}

					const FVector A2 = Path->GetPosUnsafe(PathEdge->Start);
					const FVector B2 = Path->GetPosUnsafe(PathEdge->End);
					FVector A = FVector::ZeroVector;
					FVector B = FVector::ZeroVector;

					FMath::SegmentDistToSegment(A1, B1, A2, B2, A, B);
					//if (A == A1 || A == B1 || B == A2 || B == B2) { return true; }

					if (FVector::DistSquared(A, B) >= Context->IntersectionDetails.ToleranceSquared) { return true; }

					PCGExCluster::FNode* StartNode = Cluster->GetEdgeStart(Edge);
					PCGExCluster::FNode* EndNode = Cluster->GetEdgeEnd(Edge);

					if (Settings->bInvert)
					{
						FPlatformAtomics::InterlockedExchange(&Edge.bValid, 1);
						FPlatformAtomics::InterlockedExchange(&StartNode->bValid, 1);
						FPlatformAtomics::InterlockedExchange(&EndNode->bValid, 1);
					}
					else
					{
						FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
						if (Settings->bAffectedEdgesAffectEndpoints)
						{
							FPlatformAtomics::InterlockedExchange(&StartNode->bValid, 0);
							FPlatformAtomics::InterlockedExchange(&EndNode->bValid, 0);
						}
					}

					return false;
				});

			if (Edge.bValid == static_cast<int8>(Settings->bInvert)) { return; }
		}
	}

	void FProcessor::PrepareSingleLoopScopeForNodes(const uint32 StartIndex, const int32 Count)
	{
		const int32 MaxIndex = StartIndex + Count;

		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;
		if (NodeFilterManager) { for (int i = StartIndex; i < MaxIndex; i++) { NodeFilterCache[i] = NodeFilterManager->Test(Nodes[i]); } }
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		if (NodeFilterCache[Index])
		{
			if (Settings->bInvert) { Node.bValid = true; }
			return;
		}


		const FPCGPoint& NodePoint = VtxDataFacade->Source->GetInPoint(Node.PointIndex);
		const FVector A1 = NodePoint.Transform.GetLocation();
		FBox PointBox = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(NodePoint).ExpandBy(Settings->NodeExpansion + Settings->IntersectionDetails.ToleranceSquared).TransformBy(NodePoint.Transform);

		for (const TSharedRef<PCGExPaths::FPath>& Path : Context->Paths)
		{
			if (!Path->Bounds.Intersect(PointBox)) { continue; }

			// Check paths
			Path->GetEdgeOctree()->FindFirstElementWithBoundsTest(
				PointBox, [&](const PCGExPaths::FPathEdge* PathEdge)
				{
					//if (Settings->bInvert) { if (Node.bValid) { return false; } }
					//else if (!Node.bValid) { return false; }

					const FVector A2 = Path->GetPosUnsafe(PathEdge->Start);
					const FVector B2 = Path->GetPosUnsafe(PathEdge->End);

					const FVector B1 = FMath::ClosestPointOnSegment(A1, A2, B2);
					const FVector C1 = Context->DistanceDetails->GetSourceCenter(NodePoint, A1, B1);

					if (FVector::DistSquared(B1, C1) >= Context->IntersectionDetails.ToleranceSquared) { return true; }

					if (Settings->bInvert)
					{
						FPlatformAtomics::InterlockedExchange(&Node.bValid, 1);
						if (Settings->bAffectedNodesAffectConnectedEdges)
						{
							for (const PCGExGraph::FLink Lk : Node.Links)
							{
								FPlatformAtomics::InterlockedExchange(&(Cluster->GetEdge(Lk))->bValid, 1);
								FPlatformAtomics::InterlockedExchange(&Cluster->GetNode(Lk)->bValid, 1);
							}
						}
					}
					else
					{
						FPlatformAtomics::InterlockedExchange(&Node.bValid, 0);
						if (Settings->bAffectedNodesAffectConnectedEdges)
						{
							for (const PCGExGraph::FLink Lk : Node.Links)
							{
								FPlatformAtomics::InterlockedExchange(&(Cluster->GetEdge(Lk))->bValid, 0);
							}
						}
					}
					return false;
				});

			if (Node.bValid == static_cast<int8>(Settings->bInvert)) { return; }
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		FPlatformAtomics::InterlockedExchange(&EdgesProcessed, 1);
		TryConsolidate();
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		FPlatformAtomics::InterlockedExchange(&NodesProcessed, 1);
		TryConsolidate();
	}

	void FProcessor::TryConsolidate()
	{
		switch (Settings->Mode)
		{
		case EPCGExCutEdgesMode::Nodes:
			if (!NodesProcessed) { return; }
			break;
		case EPCGExCutEdgesMode::Edges:
			if (!EdgesProcessed) { return; }
			break;
		case EPCGExCutEdgesMode::NodesAndEdges:
			if (!EdgesProcessed || !NodesProcessed) { return; }
			break;
		}

		if (Settings->bInvert && Settings->bKeepEdgesThatConnectValidNodes)
		{
			StartParallelLoopForRange(Cluster->Edges->Num());
		}
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Iteration);

		//if (Edge.bValid)		{			return;		}

		const PCGExCluster::FNode* StartNode = Cluster->GetEdgeStart(Edge);
		const PCGExCluster::FNode* EndNode = Cluster->GetEdgeEnd(Edge);

		if (StartNode->bValid && EndNode->bValid) { Edge.bValid = true; }
	}

	void FProcessor::CompleteWork()
	{
		TArray<PCGExGraph::FEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);

		if (ValidEdges.IsEmpty()) { return; }

		GraphBuilder->Graph->InsertEdges(ValidEdges);
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)

		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->NodeFilterFactories, FacadePreloader);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

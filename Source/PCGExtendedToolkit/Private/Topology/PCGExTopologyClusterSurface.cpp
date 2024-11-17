// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopologyClusterSurface.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE TopologyEdgesProcessor

PCGEX_INITIALIZE_ELEMENT(TopologyClusterSurface)

bool FPCGExTopologyClusterSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExTopologyEdgesProcessorElement::Boot(InContext)) { return false; }
	PCGEX_CONTEXT_AND_SETTINGS(TopologyClusterSurface)

	return true;
}

bool FPCGExTopologyClusterSurfaceElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTopologyEdgesProcessorElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TopologyClusterSurface)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExTopologyEdges::TBatch<PCGExTopologyClusterSurface::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExTopologyEdges::TBatch<PCGExTopologyClusterSurface::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();
	Context->OutputBatches();

	return Context->TryComplete();
}

namespace PCGExTopologyClusterSurface
{
	void FProcessor::PrepareLoopScopesForEdges(const TArray<uint64>& Loops)
	{
		TProcessor<FPCGExTopologyClusterSurfaceContext, UPCGExTopologyClusterSurfaceSettings>::PrepareLoopScopesForEdges(Loops);
		PCGEx::InitArray(SubTriangulations, Loops.Num());
		for (int i = 0; i < Loops.Num(); i++)
		{
			// TODO : Find a way to pre-allocate memory here
			SubTriangulations[i] = MakeShared<TArray<PCGExGeo::FTriangle>>();
		}
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
	{
		EdgeDataFacade->Fetch(StartIndex, Count);
		FilterConstrainedEdgeScope(StartIndex, Count);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		if (ConstrainedEdgeFilterCache[EdgeIndex]) { return; }
		TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

		FVector G1 = FVector::ZeroVector;
		FVector G2 = FVector::ZeroVector;

		Cluster->GetProjectedEdgeGuides(Edge.Index, *ProjectedPositions, G1, G2);

		ProcessNodeCandidate(*Cluster->GetEdgeStart(Edge), Edge, G1, LoopIdx);
		ProcessNodeCandidate(*Cluster->GetEdgeEnd(Edge), Edge, G2, LoopIdx);
	}

	bool FProcessor::ProcessNodeCandidate(
		const PCGExCluster::FNode& Node,
		const PCGExGraph::FEdge& Edge,
		const FVector& Guide,
		const int32 LoopIdx,
		const bool bSkipBinary)
	{
		if (Node.IsBinary() && bSkipBinary)
		{
			FPlatformAtomics::InterlockedExchange(&LastBinary, Node.Index);
			return false;
		}
		if (!CellsConstraints->bKeepCellsWithLeaves && Node.IsLeaf()) { return false; }

		FPlatformAtomics::InterlockedAdd(&NumAttempts, 1);
		const TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(Node.Index, Edge.Index, Guide, Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success) { return false; }

		if (Cell->Triangulate<true>(*SubTriangulations[LoopIdx].Get(), Cluster) != PCGExTopology::ETriangulationResult::Success) { return false; }

		return true;
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

			FVector G1 = FVector::ZeroVector;
			FVector G2 = FVector::ZeroVector;

			PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Cluster->GetNode(LastBinary)->Links[0].Edge);
			Cluster->GetProjectedEdgeGuides(Edge.Index, *ProjectedPositions, G1, G2);

			ProcessNodeCandidate(*Cluster->GetEdgeStart(Edge), Edge, G1, 0, false);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		EnsureRoamingClosedLoopProcessing();

		if (!BuildValidNodeLookup()) { return; }

		InternalMesh->EditMesh(
			[&](FDynamicMesh3& InMesh)
			{
				for (const TSharedPtr<TArray<PCGExGeo::FTriangle>>& SubTriangulation : SubTriangulations)
				{
					const TArray<PCGExGeo::FTriangle>& Triangles = *SubTriangulation;

					for (const PCGExGeo::FTriangle& T : Triangles)
					{
						InMesh.AppendTriangle(VerticesLookup->Get(T.Vtx[0]), VerticesLookup->Get(T.Vtx[1]), VerticesLookup->Get(T.Vtx[2]));
					}
				}
			}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::MeshTopology, false);
	}

	void FProcessor::CompleteWork()
	{
		UE_LOG(LogTemp, Warning, TEXT("Complete %llu | %d"), Settings->UID, EdgeDataFacade->Source->IOIndex)
		StartParallelLoopForEdges(128);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

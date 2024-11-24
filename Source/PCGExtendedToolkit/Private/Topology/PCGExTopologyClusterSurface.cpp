// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopologyClusterSurface.h"

#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/PolygonFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

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
		SubTriangulations.Reserve(Loops.Num());
		for (int i = 0; i < Loops.Num(); i++)
		{
			TSharedPtr<TArray<FGeometryScriptSimplePolygon>> A = MakeShared<TArray<FGeometryScriptSimplePolygon>>();
			SubTriangulations.Add(A.ToSharedRef());
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

		FindCell(*Cluster->GetEdgeStart(Edge), Edge, LoopIdx);
		FindCell(*Cluster->GetEdgeEnd(Edge), Edge, LoopIdx);
	}

	bool FProcessor::FindCell(
		const PCGExCluster::FNode& Node,
		const PCGExGraph::FEdge& Edge,
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

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(PCGExGraph::FLink(Node.Index, Edge.Index), Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success) { return false; }

		SubTriangulations[LoopIdx]->Add(Cell->Polygon);

		FPlatformAtomics::InterlockedAdd(&NumTriangulations, 1);

		return true;
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());
			PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Cluster->GetNode(LastBinary)->Links[0].Edge);
			FindCell(*Cluster->GetEdgeStart(Edge), Edge, 0, false);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		EnsureRoamingClosedLoopProcessing();

		FGeometryScriptGeneralPolygonList ClusterPolygonList;
		ClusterPolygonList.Reset();

		if (NumTriangulations == 0 && CellsConstraints->WrapperCell && Settings->Constraints.bKeepWrapperIfSolePath)
		{
			SubTriangulations[0]->Add(CellsConstraints->WrapperCell->Polygon);
			FPlatformAtomics::InterlockedAdd(&NumTriangulations, 1);
		}

		for (const TSharedRef<TArray<FGeometryScriptSimplePolygon>>& SubTriangulation : SubTriangulations)
		{
			UGeometryScriptLibrary_PolygonListFunctions::AppendPolygonList(
				ClusterPolygonList,
				UGeometryScriptLibrary_PolygonListFunctions::CreatePolygonListFromSimplePolygons(*SubTriangulation));
		}

		bool bTriangulationError = false;

		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendPolygonListTriangulation(
			GetInternalMesh(),
			Settings->Topology.PrimitiveOptions, FTransform::Identity, ClusterPolygonList, Settings->Topology.TriangulationOptions,
			bTriangulationError);

		if (bTriangulationError && !Settings->Topology.bQuietTriangulationError)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Triangulation error."));
		}

		ApplyPointData();
	}

	void FProcessor::CompleteWork()
	{
		//UE_LOG(LogTemp, Warning, TEXT("Complete %llu | %d"), Settings->UID, EdgeDataFacade->Source->IOIndex)
		StartParallelLoopForEdges(128);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

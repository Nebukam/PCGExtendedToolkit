// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopologyClusterSurface.h"

#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "GeometryScript/PolygonFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "TopologyEdgesProcessor"
#define PCGEX_NAMESPACE TopologyEdgesProcessor

PCGEX_INITIALIZE_ELEMENT(TopologyClusterSurface)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(TopologyClusterSurface)

bool FPCGExTopologyClusterSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExTopologyEdgesProcessorElement::Boot(InContext)) { return false; }
	PCGEX_CONTEXT_AND_SETTINGS(TopologyClusterSurface)

	return true;
}

bool FPCGExTopologyClusterSurfaceElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTopologyEdgesProcessorElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TopologyClusterSurface)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->SetProjectionDetails(Settings->ProjectionDetails);
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	if (Settings->OutputMode == EPCGExTopologyOutputMode::Legacy)
	{
		Context->OutputPointsAndEdges();
		Context->OutputBatches();
		Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);
	}
	else
	{
		Context->OutputBatches();
	}

	return Context->TryComplete();
}

namespace PCGExTopologyClusterSurface
{
	void FProcessor::PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExTopologyClusterSurfaceContext, UPCGExTopologyClusterSurfaceSettings>::PrepareLoopScopesForEdges(Loops);
		SubTriangulations.Reserve(Loops.Num());
		for (int i = 0; i < Loops.Num(); i++)
		{
			PCGEX_MAKE_SHARED(A, TArray<FGeometryScriptSimplePolygon>)
			SubTriangulations.Add(A.ToSharedRef());
		}
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		EdgeDataFacade->Fetch(Scope);
		FilterConstrainedEdgeScope(Scope);

		TArray<PCGExGraph::FEdge>& ClusterEdges = *Cluster->Edges;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraph::FEdge& Edge = ClusterEdges[Index];

			if (EdgeFilterCache[Index]) { return; }

			PCGEX_MAKE_SHARED(Cell, PCGExTopology::FCell, CellsConstraints.ToSharedRef())

			FindCell(*Cluster->GetEdgeStart(Edge), Edge, Scope.LoopIndex);
			FindCell(*Cluster->GetEdgeEnd(Edge), Edge, Scope.LoopIndex);
		}
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

		PCGEX_MAKE_SHARED(Cell, PCGExTopology::FCell, CellsConstraints.ToSharedRef())

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(PCGExGraph::FLink(Node.Index, Edge.Index), Cluster.ToSharedRef(), *ProjectedVtxPositions.Get());
		if (Result != PCGExTopology::ECellResult::Success) { return false; }

		SubTriangulations[LoopIdx]->Add(Cell->Polygon);

		FPlatformAtomics::InterlockedAdd(&NumTriangulations, 1);

		return true;
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			PCGEX_MAKE_SHARED(Cell, PCGExTopology::FCell, CellsConstraints.ToSharedRef())
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
		//UE_LOG(LogPCGEx, Warning, TEXT("Complete %llu | %d"), Settings->UID, EdgeDataFacade->Source->IOIndex)
		StartParallelLoopForEdges(128);
	}

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Diagrams/PCGExBuildDualGraph.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExClusterData.h"
#include "Data/PCGPointArrayData.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Helpers/PCGExRandomHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExBuildDualGraph"
#define PCGEX_NAMESPACE BuildDualGraph

PCGExData::EIOInit UPCGExBuildDualGraphSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExBuildDualGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(BuildDualGraph)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(BuildDualGraph)

bool FPCGExBuildDualGraphElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDualGraph)

	// Validate attribute names
	if (Settings->bWriteEdgeLength) { PCGEX_VALIDATE_NAME_C(Context, Settings->EdgeLengthAttributeName); }
	if (Settings->bWriteOriginalEdgeIndex) { PCGEX_VALIDATE_NAME_C(Context, Settings->OriginalEdgeIndexAttributeName); }

	PCGEX_FWD(VtxCarryOverDetails)
	PCGEX_FWD(EdgeCarryOverDetails)
	Context->VtxCarryOverDetails.Init();
	Context->EdgeCarryOverDetails.Init();

	return true;
}

bool FPCGExBuildDualGraphElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildDualGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildDualGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->bSkipCompletion = true;
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExBuildDualGraph
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildDualGraph::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const TArray<PCGExGraphs::FEdge>& ClusterEdges = *Cluster->Edges;

		// Build edge lookup: H64U(nodeA, nodeB) -> dual vertex index
		TMap<uint64, int32> EdgeToDualVtx;
		EdgeToDualVtx.Reserve(NumEdges);

		NumValidEdges = 0;
		for (const PCGExGraphs::FEdge& E : ClusterEdges)
		{
			if (!E.bValid) { continue; }
			const int32 NodeA = Cluster->NodeIndexLookup->Get(E.Start);
			const int32 NodeB = Cluster->NodeIndexLookup->Get(E.End);
			EdgeToDualVtx.Add(PCGEx::H64U(NodeA, NodeB), NumValidEdges++);
		}

		if (NumValidEdges < 2)
		{
			bIsProcessorValid = false;
			return true;
		}

		// Build DCEL
		FaceEnumerator = MakeShared<PCGExClusters::FPlanarFaceEnumerator>();
		FaceEnumerator->Build(Cluster.ToSharedRef(), Settings->ProjectionDetails);

		if (!FaceEnumerator->IsBuilt())
		{
			bIsProcessorValid = false;
			return true;
		}

		// Build dual edges via DCEL half-edge traversal
		for (const PCGExClusters::FHalfEdge& HE : FaceEnumerator->GetHalfEdges())
		{
			if (HE.NextIndex < 0) { continue; }

			const PCGExClusters::FHalfEdge& NextHE = FaceEnumerator->GetHalfEdge(HE.NextIndex);

			const int32* VtxA = EdgeToDualVtx.Find(PCGEx::H64U(HE.OriginNode, HE.TargetNode));
			const int32* VtxB = EdgeToDualVtx.Find(PCGEx::H64U(NextHE.OriginNode, NextHE.TargetNode));

			if (VtxA && VtxB && *VtxA != *VtxB) { DualEdgeHashes.Add(PCGEx::H64U(*VtxA, *VtxB)); }
		}

		if (DualEdgeHashes.IsEmpty())
		{
			bIsProcessorValid = false;
			return true;
		}

		// Create output
		TSharedPtr<PCGExData::FPointIO> DualVtxIO = Context->MainPoints->Emplace_GetRef(PCGExData::EIOInit::New);
		if (!DualVtxIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New))
		{
			bIsProcessorValid = false;
			return true;
		}

		DualVtxIO->Tags->Reset();
		DualVtxIO->IOIndex = BatchIndex;
		PCGExClusters::Helpers::CleanupClusterData(DualVtxIO);

		UPCGBasePointData* OutputPoints = DualVtxIO->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutputPoints, NumValidEdges);

		DualVtxFacade = MakeShared<PCGExData::FFacade>(DualVtxIO.ToSharedRef());

		// Write dual vertex positions (edge midpoints)
		TPCGValueRange<FTransform> OutTransforms = OutputPoints->GetTransformValueRange(true);
		TPCGValueRange<int32> OutSeeds = OutputPoints->GetSeedValueRange(true);

		int32 DualIdx = 0;
		for (const PCGExGraphs::FEdge& Edge : ClusterEdges)
		{
			if (!Edge.bValid) { continue; }

			const FVector Midpoint = (Cluster->VtxTransforms[Edge.Start].GetLocation() +
			                          Cluster->VtxTransforms[Edge.End].GetLocation()) * 0.5;

			OutTransforms[DualIdx].SetLocation(Midpoint);
			OutSeeds[DualIdx] = PCGExRandomHelpers::ComputeSpatialSeed(Midpoint);
			++DualIdx;
		}

		// Build graph
		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(DualVtxFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(DualEdgeHashes, BatchIndex);
		GraphBuilder->bInheritNodeData = false;
		GraphBuilder->EdgesIO = Context->MainEdges;
		GraphBuilder->CompileAsync(TaskManager, false, nullptr);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void FProcessor::OnRangeProcessingComplete()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Diagrams/PCGExBuildDualGraph.h"

#include "Data/PCGExData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExClusterData.h"
#include "Blenders/PCGExUnionBlender.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Math/PCGExMathDistances.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Graphs/PCGExSubGraph.h"
#include "Helpers/PCGExRandomHelpers.h"
#include "Sampling/PCGExSamplingUnionData.h"

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

		// Build edge lookup: H64U(nodeA, nodeB) -> edge index
		TMap<uint64, int32> EdgeLookup;
		EdgeLookup.Reserve(NumEdges);
		for (int32 EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx)
		{
			const PCGExGraphs::FEdge& E = ClusterEdges[EdgeIdx];
			if (!E.bValid) { continue; }

			// Edge Start/End are point indices, convert to node indices
			const int32 NodeA = Cluster->NodeIndexLookup->Get(E.Start);
			const int32 NodeB = Cluster->NodeIndexLookup->Get(E.End);
			EdgeLookup.Add(PCGEx::H64U(NodeA, NodeB), EdgeIdx);
		}

		// Build mapping from original edge index to contiguous dual vertex index
		EdgeToVtxMap.SetNumUninitialized(NumEdges);
		int32 DualVtxIndex = 0;
		for (int32 EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx)
		{
			if (ClusterEdges[EdgeIdx].bValid)
			{
				EdgeToVtxMap[EdgeIdx] = DualVtxIndex++;
			}
			else
			{
				EdgeToVtxMap[EdgeIdx] = -1;
			}
		}

		NumValidEdges = DualVtxIndex;
		if (NumValidEdges < 2)
		{
			bIsProcessorValid = false;
			return true;
		}

		// Build DCEL face enumerator
		FaceEnumerator = MakeShared<PCGExClusters::FPlanarFaceEnumerator>();
		FaceEnumerator->Build(Cluster.ToSharedRef(), Settings->ProjectionDetails);

		if (!FaceEnumerator->IsBuilt())
		{
			bIsProcessorValid = false;
			return true;
		}

		// Build dual edges using DCEL half-edge traversal
		// For each half-edge, the next half-edge shares the target node
		const TArray<PCGExClusters::FHalfEdge>& HalfEdges = FaceEnumerator->GetHalfEdges();

		for (int32 HEIdx = 0; HEIdx < HalfEdges.Num(); ++HEIdx)
		{
			const PCGExClusters::FHalfEdge& HE = HalfEdges[HEIdx];
			if (HE.NextIndex < 0) { continue; }

			const PCGExClusters::FHalfEdge& NextHE = HalfEdges[HE.NextIndex];

			// Get original edge indices using node indices
			const int32* EdgeIdxA = EdgeLookup.Find(PCGEx::H64U(HE.OriginNode, HE.TargetNode));
			const int32* EdgeIdxB = EdgeLookup.Find(PCGEx::H64U(NextHE.OriginNode, NextHE.TargetNode));

			if (!EdgeIdxA || !EdgeIdxB) { continue; }

			// Get dual vertex indices
			const int32 VtxA = EdgeToVtxMap[*EdgeIdxA];
			const int32 VtxB = EdgeToVtxMap[*EdgeIdxB];

			if (VtxA < 0 || VtxB < 0 || VtxA == VtxB) { continue; }

			// The shared node is the target of current half-edge (= origin of next)
			const int32 SharedNode = HE.TargetNode;

			const uint64 DualHash = PCGEx::H64U(VtxA, VtxB);
			if (DualEdgeHashes.Contains(DualHash)) { continue; }

			DualEdgeHashes.Add(DualHash);
			DualEdgeToSharedNode.Add(DualHash, SharedNode);
		}

		if (DualEdgeHashes.IsEmpty())
		{
			bIsProcessorValid = false;
			return true;
		}

		// Initialize cluster output on the existing VtxDataFacade pattern (like Voronoi)
		// Create a new output for the dual vertices
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

		for (int32 EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx)
		{
			const int32 DualIdx = EdgeToVtxMap[EdgeIdx];
			if (DualIdx < 0) { continue; }

			const PCGExGraphs::FEdge& Edge = ClusterEdges[EdgeIdx];

			// Get edge endpoint positions (Edge.Start/End are point indices, VtxTransforms is indexed by point index)
			const FVector& StartPos = Cluster->VtxTransforms[Edge.Start].GetLocation();
			const FVector& EndPos = Cluster->VtxTransforms[Edge.End].GetLocation();

			// Position at edge midpoint
			const FVector Midpoint = (StartPos + EndPos) * 0.5;

			OutTransforms[DualIdx].SetLocation(Midpoint);
			OutSeeds[DualIdx] = PCGExRandomHelpers::ComputeSpatialSeed(Midpoint);
		}

		// Build graph
		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(DualVtxFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(DualEdgeHashes, BatchIndex);

		// Use Morton sorting (same as Voronoi)
		GraphBuilder->bInheritNodeData = false;
		GraphBuilder->EdgesIO = Context->MainEdges;

		// Compile without writing facade (we already wrote transforms directly)
		GraphBuilder->CompileAsync(TaskManager, false, nullptr);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		// Not used - we do all work in Process()
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		// Not used - we do all work in Process()
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

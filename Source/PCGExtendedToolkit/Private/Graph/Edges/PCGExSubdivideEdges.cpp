// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExSubdivideEdges.h"


#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideEdges"
#define PCGEX_NAMESPACE SubdivideEdges

PCGExData::EIOInit UPCGExSubdivideEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGExData::EIOInit UPCGExSubdivideEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExSubdivideEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SubdivideEdges)

bool FPCGExSubdivideEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SubdivideEdges)

	if (Settings->bFlagSubVtx) { PCGEX_VALIDATE_NAME(Settings->SubVtxFlagName) }
	if (Settings->bFlagSubEdge) { PCGEX_VALIDATE_NAME(Settings->SubEdgeFlagName) }
	if (Settings->bWriteVtxAlpha) { PCGEX_VALIDATE_NAME(Settings->VtxAlphaAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExDataBlending::SourceOverridesBlendingOps)

	return true;
}

bool FPCGExSubdivideEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SubdivideEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExSubdivideEdges::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSubdivideEdges::FBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraph::State_ReadyToCompile)

	if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSubdivideEdges
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, VtxDataFacade->Source, NodeIndexLookup,
			true, false, false);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSubdivideEdges::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		SubBlending = Context->Blending->CreateOperation();
		PCGEx::InitArray(Subdivisions, EdgeDataFacade->GetNum());
		SubdivisionPoints.Init(nullptr, EdgeDataFacade->GetNum());

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExGraph::FEdge>& ClusterEdges = *Cluster->Edges;

		EdgeDataFacade->Fetch(Scope);
		FilterEdgeScope(Scope);

		int32 NewSubdivNum = 0;
		int32 NewEdges = 0;

#define PCGEX_PROCESS_EDGE \
		if (!EdgeFilterCache[Index]) { continue; } \
		PCGExGraph::FEdge& Edge = ClusterEdges[Index]; \
		DirectionSettings.SortEndpoints(Cluster.Get(), Edge); \
		const PCGExCluster::FNode* StartNode = Cluster->GetEdgeStart(Edge); \
		const PCGExCluster::FNode* EndNode = Cluster->GetEdgeEnd(Edge); \
		FSubdivision& Sub = Subdivisions[Index];

		TSharedPtr<TArray<FVector>> Subdivs = MakeShared<TArray<FVector>>();

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!EdgeFilterCache[Index]) { continue; }

			PCGExGraph::FEdge& Edge = ClusterEdges[Index];
			DirectionSettings.SortEndpoints(Cluster.Get(), Edge);
			const PCGExCluster::FNode* StartNode = Cluster->GetEdgeStart(Edge);
			const PCGExCluster::FNode* EndNode = Cluster->GetEdgeEnd(Edge);

			FSubdivision& Sub = Subdivisions[Index];

			Sub.NumSubdivisions = 0;
			//Sub.Dir = Cluster->GetPos(EndNode) - Cluster->GetPos(StartNode);

			if (Sub.NumSubdivisions != 0)
			{
				SubdivisionPoints[Index] = Subdivs;
				NewSubdivNum += Sub.NumSubdivisions;
				NewEdges += Sub.NumSubdivisions + 1;

				Subdivs = MakeShared<TArray<FVector>>();
			}
		}

		FPlatformAtomics::InterlockedAdd(&NewNodesNum, NewSubdivNum);
		FPlatformAtomics::InterlockedAdd(&NewEdgesNum, NewEdges);
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		// TODO : Append new points

		// Add all nodes at once
		int32 StartNodeIndex = 0;
		GraphBuilder->Graph->AddNodes(NewNodesNum, StartNodeIndex);

		TSet<uint64> NewEdges;
		NewEdges.Reserve(NewEdgesNum);

		for (FSubdivision& Subdivision : Subdivisions)
		{
			if (Subdivision.NumSubdivisions == 0) { continue; }

			Subdivision.StartNodeIndex = StartNodeIndex;
			StartNodeIndex += Subdivision.NumSubdivisions;
		}
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
		IProcessor::Write();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SubdivideEdges)

		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SubdivideEdges)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(ExecutionContext, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			bIsBatchValid = false;
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

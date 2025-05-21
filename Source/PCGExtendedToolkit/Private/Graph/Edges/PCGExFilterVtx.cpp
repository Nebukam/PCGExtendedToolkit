// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExFilterVtx.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExFilterVtx"
#define PCGEX_NAMESPACE FilterVtx

TArray<FPCGPinProperties> UPCGExFilterVtxSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_FACTORIES(PCGExGraph::SourceVtxFiltersLabel, "Vtx filters.", Required, {})

	if (Mode == EPCGExVtxFilterOutput::Clusters)
	{
		PCGEX_PIN_FACTORIES(PCGExGraph::SourceEdgeFiltersLabel, "Optional Edge filters. Selected edges will be invalidated, possibly pruning more vtx along the way.", Normal, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFilterVtxSettings::OutputPinProperties() const
{
	if (Mode != EPCGExVtxFilterOutput::Points)
	{
		return Super::OutputPinProperties();
	}

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Vtx points that passed the filters.", Required, {})
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Vtx points that didn't pass the filters.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExFilterVtxSettings::GetMainOutputInitMode() const
{
	switch (Mode)
	{
	default:
	case EPCGExVtxFilterOutput::Clusters: return PCGExData::EIOInit::New;
	case EPCGExVtxFilterOutput::Points: return PCGExData::EIOInit::None;
	case EPCGExVtxFilterOutput::Attribute: return PCGExData::EIOInit::Duplicate;
	}
}

PCGExData::EIOInit UPCGExFilterVtxSettings::GetEdgeOutputInitMode() const
{
	switch (Mode)
	{
	default:
	case EPCGExVtxFilterOutput::Attribute:
	case EPCGExVtxFilterOutput::Clusters: return PCGExData::EIOInit::Forward;
	case EPCGExVtxFilterOutput::Points: return PCGExData::EIOInit::None;
	}
}

PCGEX_INITIALIZE_ELEMENT(FilterVtx)

bool FPCGExFilterVtxElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FilterVtx)

	Context->bWantsClusters = Settings->Mode != EPCGExVtxFilterOutput::Points;

	PCGEX_FWD(GraphBuilderDetails)

	if (!GetInputFactories(
		Context, PCGExGraph::SourceVtxFiltersLabel,
		Context->VtxFilterFactories, PCGExFactories::ClusterNodeFilters, true))
	{
		return false;
	}

	if (Settings->Mode == EPCGExVtxFilterOutput::Clusters)
	{
		GetInputFactories(
			Context, PCGExGraph::SourceEdgeFiltersLabel,
			Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}

	if (!Context->bWantsClusters)
	{
		Context->Inside = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->Outside = MakeShared<PCGExData::FPointIOCollection>(Context);

		Context->Inside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
		Context->Outside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;

		if (Settings->bSwap)
		{
			Context->Inside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;
			Context->Outside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
		}
	}

	// TODO : Offer to output discarded vtxs in a third output when using cluster mode

	return true;
}

bool FPCGExFilterVtxElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFilterVtxElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FilterVtx)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFilterVtx::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFilterVtx::FBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(Settings->Mode == EPCGExVtxFilterOutput::Clusters ? PCGExGraph::State_ReadyToCompile : PCGEx::State_Done)

	if (Settings->Mode == EPCGExVtxFilterOutput::Clusters)
	{
		if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }
		Context->MainPoints->StageOutputs();
	}
	else if (Settings->Mode == EPCGExVtxFilterOutput::Attribute)
	{
		Context->OutputPointsAndEdges();
	}
	else
	{
		Context->Inside->StageOutputs();
		Context->Outside->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExFilterVtx
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a light working copy with nodes only, will be deleted.
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			true, false, false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFilterVtx::Process);

		VtxFilterFactories = &Context->VtxFilterFactories;   // So filters can be initialized
		EdgeFilterFactories = &Context->EdgeFilterFactories; // So filters can be initialized

		bAllowEdgesDataFacadeScopedGet = Context->bScopedAttributeGet;
		
		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!VtxFiltersManager)
		{
			// Log error that filters shouldn't be empty
			// Also boot should've thrown :/
			return false;
		}

		if (Settings->Mode == EPCGExVtxFilterOutput::Attribute)
		{
			TestResults = VtxDataFacade->GetWritable<bool>(Settings->ResultAttributeName, !Settings->bInvert, true, PCGExData::EBufferInit::New);
			if (!TestResults) { return false; }
		}

		StartParallelLoopForNodes();
		if (!Context->EdgeFilterFactories.IsEmpty()) { StartParallelLoopForEdges(); }

		return true;
	}

	void FProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExFilterVtxContext, UPCGExFilterVtxSettings>::PrepareLoopScopesForNodes(Loops);
		ScopedPassNum = MakeShared<PCGExMT::TScopedNumericValue<int32>>(Loops, 0);
		ScopedFailNum = MakeShared<PCGExMT::TScopedNumericValue<int32>>(Loops, 0);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope)
	{
		const bool bTestResult = VtxFiltersManager->Test(Node) ? !Settings->bInvert : Settings->bInvert;

		if (bTestResult) { ScopedPassNum->GetMutable(Scope)++; }
		else { ScopedFailNum->GetMutable(Scope)++; }

		if (TestResults) { TestResults->GetMutable(Node.PointIndex) = bTestResult; }
		else { Node.bValid = bTestResult; }
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope)
	{
		EdgeDataFacade->Fetch(Scope);
		FilterEdgeScope(Scope);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope)
	{
		Edge.bValid = EdgeFilterCache[EdgeIndex] ? !Settings->bInvertEdgeFilters : Settings->bInvertEdgeFilters;
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Mode == EPCGExVtxFilterOutput::Attribute)
		{
			return;
		}

		PassNum = ScopedPassNum->Sum();
		FailNum = ScopedFailNum->Sum();

		if (Settings->Mode == EPCGExVtxFilterOutput::Clusters)
		{
			TArray<PCGExGraph::FEdge> ValidEdges;
			Cluster->GetValidEdges(ValidEdges);

			if (ValidEdges.IsEmpty()) { return; }

			GraphBuilder->Graph->InsertEdges(ValidEdges);
		}
		else if (Settings->Mode == EPCGExVtxFilterOutput::Points)
		{
			const TArray<FPCGPoint>& InPoints = VtxDataFacade->GetIn()->GetPoints();
			const TArray<PCGExCluster::FNode> Nodes = *Cluster->Nodes.Get();

			if (PassNum == 0 || FailNum == 0)
			{
				// Create a single partition in the right bucket

				const TSharedPtr<PCGExData::FPointIO> OutIO = (PassNum == 0 ? Context->Outside : Context->Inside)->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);

				if (!OutIO) { return; }

				PCGExGraph::CleanupVtxData(OutIO);

				TArray<FPCGPoint>& MutablePoints = OutIO->GetMutablePoints();
				MutablePoints.SetNumUninitialized(NumNodes);

				OutIO->IOIndex = VtxDataFacade->Source->IOIndex * 100000 + BatchIndex;
				for (int i = 0; i < NumNodes; i++) { MutablePoints[i] = InPoints[Nodes[i].PointIndex]; }

				return;
			}

			const TSharedPtr<PCGExData::FPointIO> Inside = Context->Inside->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);
			const TSharedPtr<PCGExData::FPointIO> Outside = Context->Outside->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);

			if (!Inside || !Outside) { return; }

			PCGExGraph::CleanupVtxData(Inside);
			PCGExGraph::CleanupVtxData(Outside);

			TArray<FPCGPoint>& MutableInPoints = Inside->GetMutablePoints();
			TArray<FPCGPoint>& MutableOutPoints = Outside->GetMutablePoints();

			MutableInPoints.SetNumUninitialized(PassNum);
			MutableOutPoints.SetNumUninitialized(FailNum);

			int32 PassIndex = 0;
			int32 FailIndex = 0;

			Inside->IOIndex = VtxDataFacade->Source->IOIndex * 100000 + BatchIndex;
			Outside->IOIndex = VtxDataFacade->Source->IOIndex * 100000 + BatchIndex;

			for (int i = 0; i < NumNodes; i++)
			{
				const PCGExCluster::FNode& Node = Nodes[i];
				if (Node.bValid) { MutableInPoints[PassIndex++] = InPoints[Node.PointIndex]; }
				else { MutableOutPoints[FailIndex++] = InPoints[Node.PointIndex]; }
			}
		}
	}

	void FBatch::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FilterVtx)

		if (Context->bWantsClusters ||
			Settings->bSplitOutputsByConnectivity)
		{
			TBatch<FProcessor>::CompleteWork();
			return;
		}

		// Since we don't split outputs by connectivity, we can handle filtering here directly without
		// going back to processors

		int32 PassNum = 0;
		int32 FailNum = 0;

		for (const TSharedRef<FProcessor>& P : Processors)
		{
			PassNum += P->ScopedPassNum->Sum();
			FailNum += P->ScopedFailNum->Sum();
		}

		if (PassNum == 0 || FailNum == 0)
		{
			// Just duplicate vtx points into the right bucket

			const TSharedPtr<PCGExData::FPointIO> OutIO = (PassNum == 0 ? Context->Outside : Context->Inside)->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::Duplicate);

			if (!OutIO) { return; }

			PCGExGraph::CleanupVtxData(OutIO);

			return;
		}

		// Distribute points to partitions

		const TSharedPtr<PCGExData::FPointIO> Inside = Context->Inside->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);
		const TSharedPtr<PCGExData::FPointIO> Outside = Context->Outside->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);

		if (!Inside || !Outside) { return; }

		PCGExGraph::CleanupVtxData(Inside);
		PCGExGraph::CleanupVtxData(Outside);

		TArray<FPCGPoint>& MutableInPoints = Inside->GetMutablePoints();
		TArray<FPCGPoint>& MutableOutPoints = Outside->GetMutablePoints();

		MutableInPoints.SetNumUninitialized(PassNum);
		MutableOutPoints.SetNumUninitialized(FailNum);

		int32 PassIndex = 0;
		int32 FailIndex = 0;

		Inside->IOIndex = VtxDataFacade->Source->IOIndex;
		Outside->IOIndex = VtxDataFacade->Source->IOIndex;

		for (const TSharedRef<FProcessor>& P : Processors)
		{
			const TArray<FPCGPoint>& InPoints = VtxDataFacade->GetIn()->GetPoints();
			const TArray<PCGExCluster::FNode> Nodes = *P->Cluster->Nodes.Get();

			// We need min/max numbers to be updated
			for (int i = 0; i < P->NumNodes; i++)
			{
				const PCGExCluster::FNode& Node = Nodes[i];
				if (Node.bValid) { MutableInPoints[PassIndex++] = InPoints[Node.PointIndex]; }
				else { MutableOutPoints[FailIndex++] = InPoints[Node.PointIndex]; }
			}
		}
	}

	void FBatch::Write()
	{
		VtxDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

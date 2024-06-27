// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteVtxExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Edges/Extras/PCGExVtxExtraFactoryProvider.h"

#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteVtxExtras

TArray<FPCGPinProperties> UPCGExWriteVtxExtrasSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExVtxExtra::SourceExtrasLabel, "Extra attribute handlers.", Normal, {})
	return PinProperties;
}

PCGExData::EInit UPCGExWriteVtxExtrasSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExWriteVtxExtrasSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(WriteVtxExtras)

FPCGExWriteVtxExtrasContext::~FPCGExWriteVtxExtrasContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteVtxExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteVtxExtras)

	PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories(InContext, PCGExVtxExtra::SourceExtrasLabel, Context->ExtraFactories, {PCGExFactories::EType::VtxExtra}, false);

	return true;
}

bool FPCGExWriteVtxExtrasElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteVtxExtrasElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteVtxExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExWriteVtxExtras::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExWriteVtxExtras::FProcessorBatch* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->TryComplete();
}

namespace PCGExWriteVtxExtras
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(ProjectedCluster)
		ExtraOperations->Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxExtras)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		if (!ExtraOperations->IsEmpty())
		{
			for (UPCGExVtxExtraOperation* Op : (*ExtraOperations)) { Op->PrepareForCluster(Context, BatchIndex, Cluster, VtxDataCache, EdgeDataCache); }
		}

		if (VtxNormalWriter)
		{
			ProjectedCluster = new PCGExCluster::FClusterProjection(Cluster, ProjectionSettings);
			StartParallelLoopForRange(Cluster->Nodes->Num());
			//ProjectedCluster->Build();
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		ProjectedCluster->Nodes[Iteration].Project(Cluster, ProjectionSettings);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		if (VtxNormalWriter)
		{
			PCGExCluster::FNodeProjection& VtxPt = ProjectedCluster->Nodes[Node.NodeIndex];
			VtxPt.ComputeNormal(Cluster);
			VtxNormalWriter->Values[VtxPt.Node->NodeIndex] = VtxPt.Normal;
		}

		if (VtxEdgeCountWriter) { VtxEdgeCountWriter->Values[Node.PointIndex] = Node.Adjacency.Num(); }

		if (ExtraOperations->IsEmpty()) { return; }

		TArray<PCGExCluster::FAdjacencyData> Adjacency;
		GetAdjacencyData(Cluster, Node, Adjacency);

		for (UPCGExVtxExtraOperation* Op : (*ExtraOperations)) { Op->ProcessNode(BatchIndex, Cluster, Node, Adjacency); }
	}

	void FProcessor::CompleteWork()
	{
		StartParallelLoopForNodes();
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		PCGEX_SETTINGS(WriteVtxExtras)
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DELETE)
		for (UPCGExVtxExtraOperation* Op : ExtraOperations) { PCGEX_DELETE_OPERATION(Op) }

		ProjectionSettings.Cleanup();
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxExtras)

		if (!TBatch::PrepareProcessing()) { return false; }

		{
			PCGExData::FPointIO* OutputIO = VtxIO;
			PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_FWD_INIT)
		}

		ProjectionSettings = Settings->ProjectionSettings;
		ProjectionSettings.Init(VtxIO);

		for (const UPCGExVtxExtraFactoryBase* Factory : TypedContext->ExtraFactories)
		{
			UPCGExVtxExtraOperation* NewOperation = Factory->CreateOperation();
			if (!NewOperation->PrepareForVtx(Context, VtxIO, VtxDataCache))
			{
				PCGEX_DELETE_OPERATION(NewOperation)
				continue;
			}
			NewOperation->ClusterReserve(Edges.Num());
			ExtraOperations.Add(NewOperation);
		}

		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		PCGEX_SETTINGS(WriteVtxExtras)

		ClusterProcessor->ProjectionSettings = &ProjectionSettings;
		ClusterProcessor->ExtraOperations = &ExtraOperations;

#define PCGEX_FWD_VTX(_NAME, _TYPE) ClusterProcessor->_NAME##Writer = _NAME##Writer;
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_FWD_VTX)
#undef PCGEX_ASSIGN_AXIS_GETTER

		return true;
	}

	void FProcessorBatch::Write()
	{
		TBatch<FProcessor>::Write();

		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_WRITE)
		for (UPCGExVtxExtraOperation* Op : ExtraOperations) { Op->Write(AsyncManagerPtr); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

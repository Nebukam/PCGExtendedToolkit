// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteVtxProperties.h"

#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteVtxProperties

TArray<FPCGPinProperties> UPCGExWriteVtxPropertiesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExVtxProperty::SourcePropertyLabel, "Extra attribute handlers.", Normal, {})
	return PinProperties;
}

PCGExData::EInit UPCGExWriteVtxPropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExWriteVtxPropertiesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(WriteVtxProperties)

FPCGExWriteVtxPropertiesContext::~FPCGExWriteVtxPropertiesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteVtxPropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteVtxProperties)

	PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories(
		InContext, PCGExVtxProperty::SourcePropertyLabel, Context->ExtraFactories,
		{PCGExFactories::EType::VtxProperty}, false);

	return true;
}

bool FPCGExWriteVtxPropertiesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteVtxPropertiesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteVtxProperties)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExWriteVtxProperties::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExWriteVtxProperties::FProcessorBatch* NewBatch)
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

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteVtxProperties
{
	FProcessor::~FProcessor()
	{
		ExtraOperations->Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteVtxProperties::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxProperties)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		if (!ExtraOperations->IsEmpty())
		{
			for (UPCGExVtxPropertyOperation* Op : (*ExtraOperations)) { Op->PrepareForCluster(Context, BatchIndex, Cluster, VtxDataFacade, EdgeDataFacade); }
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		if (VtxEdgeCountWriter) { VtxEdgeCountWriter->Values[Node.PointIndex] = Node.Adjacency.Num(); }

		if (ExtraOperations->IsEmpty()) { return; }

		TArray<PCGExCluster::FAdjacencyData> Adjacency;
		GetAdjacencyData(Cluster, Node, Adjacency);

		if (VtxNormalWriter) { Node.ComputeNormal(Cluster, Adjacency, VtxNormalWriter->Values[Node.PointIndex]); }

		for (UPCGExVtxPropertyOperation* Op : (*ExtraOperations)) { Op->ProcessNode(BatchIndex, Cluster, Node, Adjacency); }
	}

	void FProcessor::CompleteWork()
	{
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		PCGEX_SETTINGS(WriteVtxProperties)
		for (UPCGExVtxPropertyOperation* Op : ExtraOperations) { PCGEX_DELETE_OPERATION(Op) }
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxProperties)

		if (!TBatch::PrepareProcessing()) { return false; }

		{
			PCGExData::FFacade* OutputFacade = VtxDataFacade;
			PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_INIT)
		}

		for (const UPCGExVtxPropertyFactoryBase* Factory : TypedContext->ExtraFactories)
		{
			UPCGExVtxPropertyOperation* NewOperation = Factory->CreateOperation();
			if (!NewOperation->PrepareForVtx(Context, VtxDataFacade))
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
		PCGEX_SETTINGS(WriteVtxProperties)

		ClusterProcessor->ExtraOperations = &ExtraOperations;

#define PCGEX_FWD_VTX(_NAME, _TYPE) ClusterProcessor->_NAME##Writer = _NAME##Writer;
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_FWD_VTX)
#undef PCGEX_ASSIGN_AXIS_GETTER

		return true;
	}

	void FProcessorBatch::Write()
	{
		//	TBatch<FProcessor>::Write();
		VtxDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

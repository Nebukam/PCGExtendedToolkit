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
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExWriteVtxProperties::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExWriteVtxProperties::FProcessorBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters(PCGExMT::State_Done)) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteVtxProperties
{
	FProcessor::~FProcessor()
	{
		ExtraOperations->Empty();
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteVtxProperties::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!ExtraOperations->IsEmpty())
		{
			for (UPCGExVtxPropertyOperation* Op : (*ExtraOperations)) { Op->PrepareForCluster(ExecutionContext, BatchIndex, Cluster, VtxDataFacade, EdgeDataFacade); }
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		if (VtxEdgeCountWriter) { VtxEdgeCountWriter->GetMutable(Node.PointIndex) = Node.Adjacency.Num(); }

		if (ExtraOperations->IsEmpty()) { return; }

		TArray<PCGExCluster::FAdjacencyData> Adjacency;
		GetAdjacencyData(Cluster.Get(), Node, Adjacency);

		if (VtxNormalWriter) { Node.ComputeNormal(Cluster.Get(), Adjacency, VtxNormalWriter->GetMutable(Node.PointIndex)); }

		for (UPCGExVtxPropertyOperation* Op : (*ExtraOperations)) { Op->ProcessNode(BatchIndex, Cluster.Get(), Node, Adjacency); }
	}

	void FProcessor::CompleteWork()
	{
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		for (UPCGExVtxPropertyOperation* Op : ExtraOperations) { PCGEX_DELETE_OPERATION(Op) }
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxProperties)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;
			PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_INIT)
		}

		for (const UPCGExVtxPropertyFactoryBase* Factory : Context->ExtraFactories)
		{
			UPCGExVtxPropertyOperation* NewOperation = Factory->CreateOperation(Context);
			if (!NewOperation->PrepareForVtx(Context, VtxDataFacade))
			{
				PCGEX_DELETE_OPERATION(NewOperation)
				continue;
			}
			NewOperation->ClusterReserve(Edges.Num());
			ExtraOperations.Add(NewOperation);
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FProcessorBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ExtraOperations = &ExtraOperations;

#define PCGEX_FWD_VTX(_NAME, _TYPE, _DEFAULT_VALUE) ClusterProcessor->_NAME##Writer = _NAME##Writer;
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_FWD_VTX)
#undef PCGEX_ASSIGN_AXIS_GETTER

		return true;
	}

	void FProcessorBatch::Write()
	{
		//	TBatch<FProcessor>::Write();
		VtxDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

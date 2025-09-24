// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteVtxProperties.h"


#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"

#define LOCTEXT_NAMESPACE "WriteVtxProperties"
#define PCGEX_NAMESPACE WriteVtxProperties

TArray<FPCGPinProperties> UPCGExWriteVtxPropertiesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExVtxProperty::SourcePropertyLabel, "Extra attribute handlers.", Normal, FPCGExVtxPropertyDataTypeInfo)
	return PinProperties;
}

PCGExData::EIOInit UPCGExWriteVtxPropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExWriteVtxPropertiesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(WriteVtxProperties)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(WriteVtxProperties)

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
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteVtxProperties
{
	FProcessor::~FProcessor()
	{
		Operations.Empty();
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteVtxProperties::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		for (const UPCGExVtxPropertyFactoryData* Factory : Context->ExtraFactories)
		{
			TSharedPtr<FPCGExVtxPropertyOperation> NewOperation = Factory->CreateOperation(Context);

			if (!NewOperation->PrepareForCluster(Context, Cluster, VtxDataFacade, EdgeDataFacade)) { return false; }
			Operations.Add(NewOperation);
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExCluster::FNode& Node = Nodes[Index];

			if (VtxEdgeCountWriter) { VtxEdgeCountWriter->SetValue(Node.PointIndex, Node.Num()); }

			TArray<PCGExCluster::FAdjacencyData> Adjacency;
			GetAdjacencyData(Cluster.Get(), Node, Adjacency);
			if (VtxNormalWriter) { VtxNormalWriter->SetValue(Node.PointIndex, Node.ComputeNormal(Cluster.Get(), Adjacency)); }

			for (const TSharedPtr<FPCGExVtxPropertyOperation>& Op : Operations) { Op->ProcessNode(Node, Adjacency); }
		}
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExWriteVtxPropertiesContext, UPCGExWriteVtxPropertiesSettings>::Cleanup();
		Operations.Empty();
	}

	//////// BATCH

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxProperties)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;
			PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_INIT)
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }

		PCGEX_TYPED_PROCESSOR

#define PCGEX_FWD_VTX(_NAME, _TYPE, _DEFAULT_VALUE) TypedProcessor->_NAME##Writer = _NAME##Writer;
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_FWD_VTX)
#undef PCGEX_ASSIGN_AXIS_GETTER

		return true;
	}

	void FBatch::Write()
	{
		VtxDataFacade->WriteFastest(AsyncManager);
		TBatch<FProcessor>::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

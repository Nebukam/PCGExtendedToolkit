// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteVtxExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Edges/Extras/PCGExVtxExtraFactoryProvider.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"
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
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExWriteVtxExtras
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(ProjectedCluster)

		for (UPCGExVtxExtraOperation* Op : ExtraOperations) { PCGEX_DELETE_OPERATION(Op) }
		ExtraOperations.Empty();
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxExtras)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		for (const UPCGExVtxExtraFactoryBase* Factory : TypedContext->ExtraFactories)
		{
			UPCGExVtxExtraOperation* NewOperation = Factory->CreateOperation();
			if (!NewOperation->PrepareForCluster(Context, Cluster))
			{
				PCGEX_DELETE_OPERATION(NewOperation)
				continue;
			}

			ExtraOperations.Add(NewOperation);
		}

		if (VtxNormalWriter)
		{
			ProjectedCluster = new PCGExCluster::FClusterProjection(Cluster, ProjectionSettings);
			ProjectedCluster->Build();
		}

		if (VtxNormalWriter || VtxEdgeCountWriter) { StartParallelLoopForNodes(); }

		Cluster->GetNodePointIndices(NodePointIndices);
		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node)
	{
		if (VtxNormalWriter)
		{
			PCGExCluster::FNodeProjection& VtxPt = ProjectedCluster->Nodes[Node.NodeIndex];
			VtxPt.ComputeNormal(Cluster);
			VtxNormalWriter->Values[VtxPt.Node->NodeIndex] = VtxPt.Normal;
		}

		if (VtxEdgeCountWriter) { VtxEdgeCountWriter->Values[Node.PointIndex] = Node.Adjacency.Num(); }

		if (ExtraOperations.IsEmpty()) { return; }


		TArray<PCGExCluster::FAdjacencyData> Adjacency;
		GetAdjacencyData(Cluster, Node, Adjacency);

		for (UPCGExVtxExtraOperation* Op : ExtraOperations) { Op->ProcessNode(Node, Adjacency); }
	}

	void FProcessor::CompleteWork()
	{
		for (UPCGExVtxExtraOperation* Op : ExtraOperations) { Op->Write(NodePointIndices); }
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
		ProjectionSettings.Cleanup();
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteVtxExtras)

		if (!TBatch::PrepareProcessing()) { return false; }

		{
			PCGExData::FPointIO& OutputIO = *VtxIO;
			PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_FWD_INIT)
		}

		ProjectionSettings = Settings->ProjectionSettings;
		ProjectionSettings.Init(VtxIO);
		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		PCGEX_SETTINGS(WriteVtxExtras)

		ClusterProcessor->ProjectionSettings = &ProjectionSettings;

#define PCGEX_FWD_VTX(_NAME, _TYPE) ClusterProcessor->_NAME##Writer = _NAME##Writer;
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_FWD_VTX)
#undef PCGEX_ASSIGN_AXIS_GETTER

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		TBatch<FProcessor>::CompleteWork();
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_WRITE)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindNodeStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindNodeStates

int32 UPCGExFindNodeStatesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFindNodeStatesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExFindNodeStatesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

TArray<FPCGPinProperties> UPCGExFindNodeStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExCluster::SourceNodeStateLabel, "Node states.", Required, {})
	return PinProperties;
}

FPCGExFindNodeStatesContext::~FPCGExFindNodeStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateFactories.Empty();
	NodeIndices.Empty();
}


PCGEX_INITIALIZE_ELEMENT(FindNodeStates)

bool FPCGExFindNodeStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindNodeStates)

	return PCGExDataState::GetInputStateFactories(
		Context, PCGExCluster::SourceNodeStateLabel,
		Context->StateFactories, {PCGExFactories::EType::NodeState}, Settings->bAllowStateOverlap);
}

bool FPCGExFindNodeStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindNodeStatesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindNodeStates)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExGraph::State_ReadyForNextEdges); }
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->StatesManager)
		Context->NodeIndices.Empty();

		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		Context->CurrentCluster->GetNodePointIndices(Context->NodeIndices);
		Context->StatesManager = new PCGExDataState::TStatesManager(Context->CurrentIO);
		Context->StatesManager->RegisterAndCapture<UPCGExNodeStateFactory>(
			Context, Context->StateFactories, [&](PCGExDataFilter::TFilter* Handler)
			{
				PCGExCluster::FNodeStateHandler* NodeStateHandler = static_cast<PCGExCluster::FNodeStateHandler*>(Handler);
				NodeStateHandler->CaptureCluster(Context, Context->CurrentCluster);
			});

		if (!Context->StatesManager->bValid)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points could not be used with any graph."));
			return false;
		}

		if (Context->StatesManager->bHasPartials)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points only have partial metadata."));
		}

		Context->CurrentIO->CreateInKeys();
		Context->StatesManager->PrepareForTesting(Context->NodeIndices);

		Context->SetState(PCGExCluster::State_ProcessingCluster);
	}

	if (Context->IsState(PCGExCluster::State_ProcessingCluster))
	{
		auto ProcessNode = [&](const int32 Index) { Context->StatesManager->Test(Context->NodeIndices[Index]); };

		if (!Context->Process(ProcessNode, Context->NodeIndices.Num())) { return false; }

		Context->SetState(PCGExGraph::State_WritingMainState);
	}

	if (Context->IsState(PCGExGraph::State_WritingMainState))
	{
		if (Settings->bWriteStateName)
		{
			Context->StatesManager->WriteStateNames(Settings->StateNameAttributeName, Settings->StatelessName, Context->NodeIndices);
		}

		if (Settings->bWriteStateValue)
		{
			Context->StatesManager->WriteStateValues(Settings->StateValueAttributeName, Settings->StatelessValue, Context->NodeIndices);
		}

		if (Settings->bWriteEachStateIndividually)
		{
			Context->StatesManager->WriteStateIndividualStates(Context->GetAsyncManager(), Context->NodeIndices);
			Context->SetAsyncState(PCGExGraph::State_WritingStatesAttributes);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingStatesAttributes);
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingStatesAttributes))
	{
		PCGEX_WAIT_ASYNC

		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->StatesManager->WritePrepareForStateAttributes(Context);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->StatesManager->WriteStateAttributes(PointIndex);
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

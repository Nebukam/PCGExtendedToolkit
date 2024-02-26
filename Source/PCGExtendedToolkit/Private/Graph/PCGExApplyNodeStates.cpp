// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExApplyNodeStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE ApplyNodeStates

int32 UPCGExApplyNodeStatesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExApplyNodeStatesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExApplyNodeStatesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExApplyNodeStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinStateParams = PinProperties.Emplace_GetRef(PCGExGraph::SourceNodeStateLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinStateParams.Tooltip = FTEXT("Node states.");
#endif

	return PinProperties;
}

FPCGExApplyNodeStatesContext::~FPCGExApplyNodeStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateDefinitions.Empty();
}


PCGEX_INITIALIZE_ELEMENT(ApplyNodeStates)

bool FPCGExApplyNodeStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ApplyNodeStates)

	return PCGExDataState::GetInputStates(
		Context,
		PCGExGraph::SourceNodeStateLabel,
		Context->StateDefinitions);
}

bool FPCGExApplyNodeStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExApplyNodeStatesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ApplyNodeStates)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->StatesManager)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->StatesManager = new PCGExDataState::AStatesManager(Context->CurrentIO);
			Context->StatesManager->Register<UPCGExNodeStateDefinition, PCGExCluster::FNodeStateHandler>(
				Context->StateDefinitions,
				[&](PCGExCluster::FNodeStateHandler* Handler)
				{
					//TODO: Capture cluster VTX
					//Handler->Capture(&Context->Graphs, Context->CurrentIO);
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
			Context->StatesManager->PrepareForTesting();

			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO) { Context->StatesManager->Test(Index); };

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExGraph::State_WritingMainState);
	}

	if (Context->IsState(PCGExGraph::State_WritingMainState))
	{
		if (Settings->bWriteStateName)
		{
			Context->StatesManager->WriteStateNames(Settings->StateNameAttributeName, Settings->StatelessName);
		}

		if (Settings->bWriteStateValue)
		{
			Context->StatesManager->WriteStateValues(Settings->StateValueAttributeName, Settings->StatelessValue);
		}

		if (Settings->bWriteEachStateIndividually)
		{
			Context->StatesManager->WriteStateIndividualStates(Context->GetAsyncManager());
			Context->SetAsyncState(PCGExGraph::State_WritingStatesAttributes);
		}
		else
		{
			Context->SetState(PCGExGraph::State_WritingStatesAttributes);
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingStatesAttributes))
	{
		PCGEX_WAIT_ASYNC
		Context->StatesManager->WriteStateAttributes(Context->GetAsyncManager());
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

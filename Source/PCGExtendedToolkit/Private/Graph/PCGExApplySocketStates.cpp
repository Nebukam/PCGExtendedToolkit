// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExApplySocketStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE ApplySocketStates

int32 UPCGExApplySocketStatesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExApplySocketStatesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExApplySocketStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinStateParams = PinProperties.Emplace_GetRef(PCGExGraph::SourceSocketStateLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinStateParams.Tooltip = FTEXT("Socket states.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExApplySocketStatesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (bDeleteCustomGraphData) { PinProperties.Pop(); }
	return PinProperties;
}

FPCGExApplySocketStatesContext::~FPCGExApplySocketStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateDefinitions.Empty();
}


PCGEX_INITIALIZE_ELEMENT(ApplySocketStates)

bool FPCGExApplySocketStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ApplySocketStates)

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGExGraph::SourceSocketStateLabel);

	TSet<FName> UniqueSocketNames;
	for (const UPCGExGraphDefinition* GraphDefinition : Context->Graphs.Params) { GraphDefinition->AddSocketNames(UniqueSocketNames); }


	TSet<FName> UniqueStatesNames;
	for (const FPCGTaggedData& InputState : Inputs)
	{
		if (const UPCGExSocketStateDefinition* State = Cast<UPCGExSocketStateDefinition>(InputState.Data))
		{
			if (UniqueStatesNames.Contains(State->StateName))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("State '{0}' has the same name as another state, it will be ignored."), FText::FromName(State->StateName)));
				continue;
			}

			if (State->Tests.IsEmpty())
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("State '{0}' has no conditions and will be ignored."), FText::FromName(State->StateName)));
				continue;
			}

			UniqueStatesNames.Add(State->StateName);
			Context->StateDefinitions.Add(const_cast<UPCGExSocketStateDefinition*>(State));
		}
	}

	UniqueSocketNames.Empty();
	UniqueStatesNames.Empty();

	if (Context->StateDefinitions.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing valid socket states."));
		return false;
	}

	return true;
}

bool FPCGExApplySocketStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExApplySocketStatesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ApplySocketStates)

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
			Context->StatesManager->Register<UPCGExSocketStateDefinition, PCGExGraph::FSocketStateHandler>(
				Context->StateDefinitions,
				[&](PCGExGraph::FSocketStateHandler* Handler)
				{
					Handler->Capture(&Context->Graphs, Context->CurrentIO);
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
		if (Settings->bDeleteCustomGraphData)
		{
			Context->MainPoints->ForEach(
				[&](const PCGExData::FPointIO& PointIO, int32)
				{
					auto DeleteSockets = [&](const UPCGExGraphDefinition* Params, int32)
					{
						const UPCGPointData* OutData = PointIO.GetOut();
						for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets) { Socket.DeleteFrom(OutData); }
						OutData->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
					};
					Context->Graphs.ForEach(Context, DeleteSockets);
				});

			Context->OutputPoints();
		}
		else
		{
			Context->OutputPointsAndGraphParams();
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

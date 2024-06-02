// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindSocketStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindSocketStates

int32 UPCGExFindSocketStatesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFindSocketStatesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExFindSocketStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExGraph::SourceSocketStateLabel, "Socket states.", Required, {})	
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindSocketStatesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (bDeleteCustomGraphData) { PinProperties.Pop(); }
	return PinProperties;
}

FPCGExFindSocketStatesContext::~FPCGExFindSocketStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateDefinitions.Empty();
	PointIndices.Empty();
}


PCGEX_INITIALIZE_ELEMENT(FindSocketStates)

bool FPCGExFindSocketStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindSocketStates)

	return PCGExDataState::GetInputStateFactories(
		Context, PCGExGraph::SourceSocketStateLabel,
		Context->StateDefinitions, {PCGExFactories::EType::SocketState}, Settings->bAllowStateOverlap);
}

bool FPCGExFindSocketStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindSocketStatesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindSocketStates)

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
			PCGEx::ArrayOfIndices(Context->PointIndices, Context->CurrentIO->GetNum());

			Context->StatesManager = new PCGExDataState::TStatesManager(Context->CurrentIO);
			Context->StatesManager->RegisterAndCapture<UPCGExSocketStateFactory>(
				Context, Context->StateDefinitions, [&](PCGExDataFilter::TFilter* Handler)
				{
					PCGExGraph::FSocketStateHandler* SocketHandler = static_cast<PCGExGraph::FSocketStateHandler*>(Handler);
					SocketHandler->CaptureGraph(&Context->Graphs, Context->CurrentIO);
				});

			if (!Context->StatesManager->bValid)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points could not be used with any graph."));
				return false;
			}

			if (Context->StatesManager->bHasPartials)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points only have partial metadata, checks relying on those will be skipped."));
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
			Context->StatesManager->WriteStateNames(Settings->StateNameAttributeName, Settings->StatelessName, Context->PointIndices);
		}

		if (Settings->bWriteStateValue)
		{
			Context->StatesManager->WriteStateValues(Settings->StateValueAttributeName, Settings->StatelessValue, Context->PointIndices);
		}

		if (Settings->bWriteEachStateIndividually)
		{
			Context->StatesManager->WriteStateIndividualStates(Context->GetAsyncManager(), Context->PointIndices);
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

		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->StatesManager->WritePrepareForStateAttributes(Context);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->StatesManager->WriteStateAttributes(PointIndex);
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
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

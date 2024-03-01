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
	FPCGPinProperties& PinStateParams = PinProperties.Emplace_GetRef(PCGExCluster::SourceNodeStateLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinStateParams.Tooltip = FTEXT("Node states.");
#endif

	return PinProperties;
}

FPCGExFindNodeStatesContext::~FPCGExFindNodeStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateDefinitions.Empty();
}


PCGEX_INITIALIZE_ELEMENT(FindNodeStates)

bool FPCGExFindNodeStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindNodeStates)

	return PCGExDataState::GetInputStates(
		Context,
		PCGExCluster::SourceNodeStateLabel,
		Context->StateDefinitions);
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
		PCGEX_DELETE(Context->StatesManager)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->StatesManager = new PCGExDataState::TStatesManager(Context->CurrentIO);
			Context->StatesManager->Register<UPCGExNodeStateDefinition>(
				Context->StateDefinitions,
				[&](PCGExDataFilter::TFilterHandler* Handler)
				{
					PCGExCluster::FNodeStateHandler* SocketHandler = static_cast<PCGExCluster::FNodeStateHandler*>(Handler);
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
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

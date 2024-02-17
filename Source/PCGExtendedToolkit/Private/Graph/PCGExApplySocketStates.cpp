// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExApplySocketStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE ApplySocketStates

int32 UPCGExApplySocketStatesSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExApplySocketStatesSettings::GetMainOutputInitMode() const { return bPruneStatelessPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExApplySocketStatesContext::~FPCGExApplySocketStatesContext()
{
	PCGEX_TERMINATE_ASYNC

	StateDefinitions.Empty();
	PCGEX_DELETE_TARRAY(StateMappings)

	for (TArray<bool>* Array : States)
	{
		Array->Empty();
		delete Array;
	}

	States.Empty();
}

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
	PinProperties.Pop(); //Remove graph output
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExApplySocketStatesSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(ApplySocketStates)

bool FPCGExApplySocketStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ApplySocketStates)

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGExGraph::SourceSocketStateLabel);

	TSet<FName> UniqueSocketNames;
	for (const UPCGExGraphDefinition* GraphDefinition : Context->Graphs.Params) { GraphDefinition->AddSocketNames(UniqueSocketNames); }

	for (const FPCGTaggedData& InputState : Inputs)
	{
		if (TObjectPtr<UPCGExSocketStateDefinition> State = Cast<UPCGExSocketStateDefinition>(InputState.Data))
		{
			//
			Context->States.Add(new TArray<bool>());
			Context->StateDefinitions.Add(State);
		}
	}

	if (Context->StateDefinitions.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing socket states."));
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
		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			const int32 NumPoints = Context->CurrentIO->GetNum();
			for (TArray<bool>* Array : Context->States)
			{
				Array->Reset(NumPoints);
				Array->SetNum(NumPoints);
			}
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		PCGEX_DELETE_TARRAY(Context->StateMappings)

		if (!Context->AdvanceGraph()) { Context->SetState(PCGExGraph::State_WritingClusters); }
		else
		{
			Context->CurrentIO->CreateInKeys();

			int32 NumValid = 0;
			int32 NumPartial = 0;
			const int32 NumStates = Context->StateDefinitions.Num();
			for (UPCGExSocketStateDefinition* Def : Context->StateDefinitions)
			{
				PCGExGraph::FSingleStateMapping* Mapping = new PCGExGraph::FSingleStateMapping(Def, Context->CurrentGraph, Context->CurrentIO);

				if (!Mapping->bValid)
				{
					Context->StateMappings.Add(nullptr);
					delete Mapping;
				}
				else
				{
					Context->StateMappings.Add(Mapping);
					NumValid++;
					if (Mapping->bPartial) { NumPartial++; }
				}
			}

			if (NumValid != NumStates)
			{
				PCGEX_GRAPH_MISSING_METADATA
				return false;
			}

			if (NumValid < NumStates || NumPartial > 0)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Some required state data is missing from '{0}'. Results may be incorrect."), FText::FromName(Context->CurrentGraph->GraphIdentifier)));
			}

			Context->SetState(PCGExGraph::State_BuildCustomGraph);
		}
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_BuildCustomGraph))
	{
		const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
		const int32 NumMappings = Context->StateMappings.Num();
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			PCGMetadataEntryKey Key = InPoints[PointIndex].MetadataEntry;

			for (int i = 0; i < NumMappings; i++)
			{
				bool bValidState = false;
				PCGExGraph::FSingleStateMapping* Mapping = Context->StateMappings[i];
				if (!Mapping) { continue; }

				const int32 NumConditions = Mapping->Definition->Conditions.Num();
				for (int j = 0; j < NumConditions; j++)
				{
					const FPCGMetadataAttribute<int32>* Attribute = Mapping->Attributes[j];
					if (!Attribute) { continue; }

					int32 EdgeValue = Attribute->GetValueFromItemKey(Key);
					const FPCGExSocketConditionDescriptor& Condition = Mapping->Definition->Conditions[i];

					// TODO : Check if condition is met 
				}

				(*Context->States[i])[PointIndex] = bValidState;
			}
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if(Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		// TODO : Apply & write states
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
						for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
						{
							Socket.DeleteFrom(OutData);
						}
						OutData->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
					};
					Context->Graphs.ForEach(Context, DeleteSockets);
				});
		}
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExEvaluatePointTask::ExecuteTask()
{
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

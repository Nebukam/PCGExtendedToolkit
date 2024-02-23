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

	PCGEX_DELETE(StateNameWriter)
	PCGEX_DELETE(StateValueWriter)

	StateDefinitions.Empty();
	HighestState.Empty();
	PCGEX_DELETE_TARRAY(StateMappings)

	for (TArray<bool>* Array : States)
	{
		Array->Empty();
		delete Array;
	}

	States.Empty();
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

			if (State->Conditions.IsEmpty())
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("State '{0}' has no conditions and will be ignored."), FText::FromName(State->StateName)));
				continue;
			}

			Context->States.Add(new TArray<bool>());
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
		PCGEX_DELETE_TARRAY(Context->States)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			const int32 NumPoints = Context->CurrentIO->GetNum();
			for (TArray<bool>* Array : Context->States)
			{
				Array->Empty();
				delete Array;
			}

			Context->States.Empty();

			int32 MappingIndex = 0;
			for (UPCGExSocketStateDefinition* Def : Context->StateDefinitions)
			{
				PCGExGraph::FSingleStateMapping* Mapping = new PCGExGraph::FSingleStateMapping(Def);
				Mapping->Capture(&Context->Graphs, Context->CurrentIO);

				if (!Mapping->bValid)
				{
					Context->StateMappings.Add(nullptr);
					delete Mapping;
				}
				else
				{
					Context->StateMappings.Add(Mapping);
					Mapping->Index = MappingIndex++;
					TArray<bool>* Array = new TArray<bool>();
					Array->SetNum(NumPoints);
					Context->States.Add(Array);
				}
			}

			if (Context->StateMappings.IsEmpty())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points could not be used with any graph."));
				return false;
			}

			bool bHasPartials = false;
			for (const PCGExGraph::FSingleStateMapping* StateMapping : Context->StateMappings) { if (StateMapping->bPartial) { bHasPartials = true; } }
			if (bHasPartials)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points only have partial metadata."));
			}

			Context->HighestState.SetNum(NumPoints);

			// Sort mappings so higher priorities come last, as they have to potential to override values.
			Context->StateMappings.Sort(
				[&](const PCGExGraph::FSingleStateMapping& A, const PCGExGraph::FSingleStateMapping& B)
				{
					return A.Definition->Priority < B.Definition->Priority;
				});

			Context->CurrentIO->CreateInKeys();
			for (PCGExGraph::FSingleStateMapping* StateMapping : Context->StateMappings) { StateMapping->Grab(Context->CurrentIO); }

			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const int32 NumStates = Context->StateMappings.Num();
		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			int32 HighestState = -1;

			for (int i = 0; i < NumStates; i++)
			{
				const PCGExGraph::FSingleStateMapping* Mapping = Context->StateMappings[i];
				const bool bValue = Mapping->Test(Index);
				(*Context->States[i])[Index] = bValue;
				if (bValue) { HighestState = i; }
			}

			Context->HighestState[Index] = HighestState;
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExGraph::State_WritingMainState);
	}

	if (Context->IsState(PCGExGraph::State_WritingMainState))
	{
		const int32 NumPoints = Context->CurrentIO->GetOutNum();

		if (Settings->bWriteStateName)
		{
			PCGEx::TFAttributeWriter<FName>* StateNameWriter = new PCGEx::TFAttributeWriter<FName>(Settings->StateNameAttributeName, Settings->StatelessName, false);
			StateNameWriter->BindAndGet(*Context->CurrentIO);
			for (int i = 0; i < NumPoints; i++)
			{
				if (const int32 HighestStateId = Context->HighestState[i]; HighestStateId != -1) { StateNameWriter->Values[i] = Context->StateMappings[HighestStateId]->Definition->StateName; }
				else { StateNameWriter->Values[i] = Settings->StatelessName; }
			}
			StateNameWriter->Write();
			PCGEX_DELETE(StateNameWriter)
		}

		if (Settings->bWriteStateValue)
		{
			PCGEx::TFAttributeWriter<int32>* StateValueWriter = new PCGEx::TFAttributeWriter<int32>(Settings->StateValueAttributeName, Settings->StatelessValue, false);
			StateValueWriter->BindAndGet(*Context->CurrentIO);
			for (int i = 0; i < NumPoints; i++)
			{
				if (const int32 HighestStateId = Context->HighestState[i]; HighestStateId != -1) { StateValueWriter->Values[i] = Context->StateMappings[HighestStateId]->Definition->StateId; }
				else { StateValueWriter->Values[i] = Settings->StatelessValue; }
			}
			StateValueWriter->Write();
			PCGEX_DELETE(StateValueWriter)
		}

		if (Settings->bWriteEachStateIndividually)
		{
			for (int i = 0; i < Context->StateMappings.Num(); i++)
			{
				Context->GetAsyncManager()->Start<FPCGExWriteIndividualStateTask>(i, Context->CurrentIO, Context->StateMappings[i]);
			}

			Context->SetAsyncState(PCGExGraph::State_WritingIndividualStates);
		}
		else
		{
			Context->SetState(PCGExGraph::State_WritingStatesAttributes);
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingIndividualStates))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_WritingStatesAttributes);
	}

	if (Context->IsState(PCGExGraph::State_WritingStatesAttributes))
	{
		const int32 NumPoints = Context->CurrentIO->GetOutNum();

		for (PCGExGraph::FSingleStateMapping* Mapping : Context->StateMappings)
		{
			Mapping->PrepareData(Context->CurrentIO, Context->States[Mapping->Index]);
			if (!Mapping->OverlappingAttributes.IsEmpty())
			{
				FString Names = FString::Join(Mapping->OverlappingAttributes.Array(), TEXT(", "));
				PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Some If/Else attributes ({0}) have the same name but different types, this will have unexpected results."), FText::FromString(Names)));
			}
		}

		for (int i = 0; i < NumPoints; i++) { Context->GetAsyncManager()->Start<FPCGExEvaluatePointTask>(i, Context->CurrentIO); }
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

bool FPCGExEvaluatePointTask::ExecuteTask()
{
	FPCGExApplySocketStatesContext* Context = static_cast<FPCGExApplySocketStatesContext*>(Manager->Context);
	PCGEX_SETTINGS(ApplySocketStates)

	const PCGMetadataEntryKey Key = PointIO->GetOutPoint(TaskIndex).MetadataEntry;

	auto ForwardValues = [&](
		TArray<FPCGMetadataAttributeBase*>& In,
		TArray<FPCGMetadataAttributeBase*>& Out)
	{
		for (int i = 0; i < Out.Num(); i++)
		{
			FPCGMetadataAttributeBase* OutAtt = Out[i];
			if (!OutAtt) { continue; }
			PCGMetadataAttribute::CallbackWithRightType(
				OutAtt->GetTypeId(),
				[&](auto DummyValue) -> void
				{
					using RawT = decltype(DummyValue);
					const FPCGMetadataAttribute<RawT>* TypedInAtt = static_cast<FPCGMetadataAttribute<RawT>*>(In[i]);
					FPCGMetadataAttribute<RawT>* TypedOutAtt = static_cast<FPCGMetadataAttribute<RawT>*>(OutAtt);
					TypedOutAtt->SetValue(Key, TypedInAtt->GetValueFromItemKey(PCGInvalidEntryKey));
				});
		}
	};

	for (PCGExGraph::FSingleStateMapping* Mapping : Context->StateMappings)
	{
		if ((*Context->States[Mapping->Index])[TaskIndex]) { ForwardValues(Mapping->InIfAttributes, Mapping->OutIfAttributes); }
		else { ForwardValues(Mapping->InElseAttributes, Mapping->OutElseAttributes); }
	}

	return true;
}

bool FPCGExWriteIndividualStateTask::ExecuteTask()
{
	FPCGExApplySocketStatesContext* Context = static_cast<FPCGExApplySocketStatesContext*>(Manager->Context);
	PCGEX_SETTINGS(ApplySocketStates)

	PCGEx::TFAttributeWriter<bool>* StateWriter = new PCGEx::TFAttributeWriter<bool>(Mapping->Definition->StateName);
	StateWriter->BindAndGet(*PointIO);

	const int32 NumPoints = PointIO->GetOutNum();
	for (int i = 0; i < NumPoints; i++) { StateWriter->Values[i] = (*Context->States[TaskIndex])[i]; }
	StateWriter->Write();

	PCGEX_DELETE(StateWriter)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

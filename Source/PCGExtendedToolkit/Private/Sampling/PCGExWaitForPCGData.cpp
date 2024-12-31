// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExWaitForPCGData.h"

#include "PCGExPointsProcessor.h"
#include "PCGGraph.h"
#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExWaitForPCGDataElement"
#define PCGEX_NAMESPACE WaitForPCGData

UPCGExWaitForPCGDataSettings::UPCGExWaitForPCGDataSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExWaitForPCGDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_DEPENDENCIES
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExWaitForPCGDataSettings::OutputPinProperties() const
{
	const UPCGGraph* PinPropertiesSource = PCGExHelpers::LoadBlocking_AnyThread(TargetGraph);

	TArray<FPCGPinProperties> PinProperties;
	if (!PinPropertiesSource) { return PinProperties; }

	TArray<FPCGPinProperties> FoundPins = PinPropertiesSource->GetOutputNode()->OutputPinProperties();
	PinProperties.Reserve(FoundPins.Num());
	for (FPCGPinProperties Pin : FoundPins)
	{
		Pin.bInvisiblePin = false;
		PinProperties.Add(Pin);
	}
	return PinProperties;
}

PCGExData::EIOInit UPCGExWaitForPCGDataSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(WaitForPCGData)

FName UPCGExWaitForPCGDataSettings::GetMainInputPin() const
{
	return PCGEx::SourceTargetsLabel;
}

bool FPCGExWaitForPCGDataElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WaitForPCGData)

	PCGEX_VALIDATE_NAME(Settings->ActorReferenceAttribute)

	UPCGGraph* GraphData = PCGExHelpers::LoadBlocking_AnyThread(Settings->TargetGraph);

	if (!GraphData) { return false; }

	TArray<FPCGPinProperties> PinProperties = GraphData->GetOutputNode()->OutputPinProperties();
	Context->RequiredPinProperties.Reserve(PinProperties.Num());

	for (FPCGPinProperties OutputPin : PinProperties)
	{
		if (OutputPin.IsRequiredPin())
		{
			Context->RequiredPinProperties.Add(OutputPin);
		}
	}

	if (Context->RequiredPinProperties.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Target graph must have at least one 'Required' output."));
		return false;
	}

	return true;
}

bool FPCGExWaitForPCGDataElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWaitForPCGDataElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WaitForPCGData)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWaitForPCGData::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWaitForPCGData::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	return Context->TryComplete();
}

namespace PCGExWaitForPCGData
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		StartTime = Context->SourceComponent->GetWorld()->GetTimeSeconds();

		ActorReferences = MakeShared<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, PointDataFacade->Source))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
			return false;
		}

		ActorReferences->Grab();
		QueuedActors.Reserve(ActorReferences->Values.Num());

		bool bHasUnresolvedReferences = false;

		for (int i = 0; i < ActorReferences->Values.Num(); i++)
		{
			if (AActor* Actor = Cast<AActor>(ActorReferences->Values[i].ResolveObject())) { QueuedActors.Add(Actor); }
			else { bHasUnresolvedReferences = true; }
		}

		if (QueuedActors.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Could not resolve any actor references."));
			return false;
		}

		if (bHasUnresolvedReferences)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some actor references could be resolved."));
		}

		StartQueue();

		return true;
	}

	void FProcessor::StartQueue()
	{
		QueuedComponents.Reset();
		QueuedComponents.SetNum(QueuedActors.Num());

		PCGEX_ASYNC_THIS_DECL

		Ticks = 0;
		for (int i = 0; i < QueuedComponents.Num(); i++)
		{
			AsyncManager->GrowNumStarted();
			AsyncTask(
				ENamedThreads::GameThread, [AsyncThis, It = i]()
				{
					PCGEX_ASYNC_THIS
					This->QueuedActors[It]->GetComponents(UPCGComponent::StaticClass(), This->QueuedComponents[It]);
					This->Ticks++;

					if (This->Ticks == This->QueuedActors.Num())
					{
						This->StartParallelLoopForRange(This->QueuedActors.Num(), 1);
					}
					
					This->AsyncManager->GrowNumCompleted();
				});
		}
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		TArray<UPCGComponent*> FoundComponents = QueuedComponents[Iteration];
		for (int i = 0; i < FoundComponents.Num(); i++)
		{
			const UPCGComponent* PCGComponent = FoundComponents[i];
			if (PCGComponent == Context->SourceComponent.Get() ||
				PCGComponent->GetGraph() != Settings->TargetGraph.Get())
			{
				FoundComponents.RemoveAt(i);
				i--;
			}
		}

		if (FoundComponents.IsEmpty()) { return; } // Wait until next run

		for (const UPCGComponent* PCGComponent : FoundComponents)
		{
			const FPCGDataCollection& GraphOutput = PCGComponent->GetGeneratedGraphOutput();
			if (GraphOutput.TaggedData.IsEmpty()) { return; }

			for (const FPCGPinProperties& RequiredPin : Context->RequiredPinProperties)
			{
				TArray<FPCGTaggedData> Datas = GraphOutput.GetInputsByPin(RequiredPin.Label);
				if (Datas.IsEmpty()) { return; } // No data, invalid graph
			}
		}

		// Has not returned! Good to go.

		QueuedActors[Iteration] = nullptr;

		for (const UPCGComponent* PCGComponent : FoundComponents)
		{
			const FPCGDataCollection& GraphOutput = PCGComponent->GetGeneratedGraphOutput();
			for (const FPCGPinProperties& RequiredPin : Context->RequiredPinProperties)
			{
				Context->StagedOutputReserve(GraphOutput.TaggedData.Num());
				for (int i = 0; i < GraphOutput.TaggedData.Num(); i++)
				{
					const FPCGTaggedData& TaggedData = GraphOutput.TaggedData[i];
					Context->StageOutput(
						RequiredPin.Label,
						const_cast<UPCGData*>(TaggedData.Data.Get()), // const_cast is fine here, we don't modify the data.
						TaggedData.Tags, false, false);
				}
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		for (int i = 0; i < QueuedActors.Num(); i++)
		{
			if (!QueuedActors[i])
			{
				QueuedActors.RemoveAt(i);
				i--;
			}
		}

		if (!QueuedActors.IsEmpty())
		{
			if (Context->SourceComponent->GetWorld()->GetTimeSeconds() - StartTime < Settings->Timeout)
			{
				StartQueue();
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Wait for PCG Data TIMEOUT."));
			}
		}
	}


	void FProcessor::CompleteWork()
	{
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

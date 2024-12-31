// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExWaitForPCGData.h"

#include "PCGExPointsProcessor.h"
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
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Reserve(ExpectedPins.Num());

	for (int i = 0; i < ExpectedPins.Num(); i++)
	{
		const FPCGExExpectedPin& ExpectedPin = ExpectedPins[i];
		PinProperties.Emplace_GetRef(ExpectedPin.Label, ExpectedPin.AllowedTypes);
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

		bSelectByTag = !Settings->Tag.IsNone();

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
			if (AActor* Actor = Cast<AActor>(ActorReferences->Values[i].ResolveObject())) { QueuedActors[i] = Actor; }
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

		StartParallelLoopForRange(QueuedActors.Num(), 1);

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		AActor* Actor = QueuedActors[Iteration];

		TArray<UPCGComponent*> FoundComponents;
		Actor->GetComponents(UPCGComponent::StaticClass(), FoundComponents);

		if (bSelectByTag)
		{
			for (int i = 0; i < FoundComponents.Num(); i++)
			{
				if (!FoundComponents[i]->ComponentHasTag(Settings->Tag))
				{
					FoundComponents.RemoveAt(i);
					i--;
				}
			}
		}

		if (FoundComponents.IsEmpty()) { return; } // Wait until next run

		for (const UPCGComponent* PCGComponent : FoundComponents)
		{
			const FPCGDataCollection& GraphOutput = PCGComponent->GetGeneratedGraphOutput();
			if (GraphOutput.TaggedData.IsEmpty()) { return; }

			for (const FPCGExExpectedPin& ExpectedPin : Settings->ExpectedPins)
			{
				TArray<FPCGTaggedData> Datas = GraphOutput.GetInputsByPin(ExpectedPin.Label);
				if (Datas.IsEmpty()) { return; } // No data
			}
		}

		// Has not returned! Good to go.

		QueuedActors[Iteration] = nullptr;

		for (const UPCGComponent* PCGComponent : FoundComponents)
		{
			const FPCGDataCollection& GraphOutput = PCGComponent->GetGeneratedGraphOutput();
			for (const FPCGExExpectedPin& ExpectedPin : Settings->ExpectedPins)
			{
				TArray<FPCGTaggedData> Datas = GraphOutput.GetInputsByPin(ExpectedPin.Label);
				for (int i = 0; i < Datas.Num(); i++)
				{
					Context->StageOutput(
						ExpectedPin.Label,
						const_cast<UPCGData*>(Datas[i].Data.Get()), // const_cast is fine here, we don't modify the data.
						Datas[i].Tags, false, false);
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

		if (!QueuedActors.IsEmpty()) { StartParallelLoopForRange(QueuedActors.Num(), 1); }
	}


	void FProcessor::CompleteWork()
	{
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

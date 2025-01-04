// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDestroyActor.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSubSystem.h"
#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExDestroyActorElement"
#define PCGEX_NAMESPACE DestroyActor

UPCGExDestroyActorSettings::UPCGExDestroyActorSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExDestroyActorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_DEPENDENCIES
	return PinProperties;
}

PCGExData::EIOInit UPCGExDestroyActorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(DestroyActor)

FName UPCGExDestroyActorSettings::GetMainInputPin() const
{
	return PCGEx::SourceTargetsLabel;
}

bool FPCGExDestroyActorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DestroyActor)

	return true;
}

bool FPCGExDestroyActorElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDestroyActorElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DestroyActor)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExDestroyActors::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExDestroyActors::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExDestroyActors
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExDestroyActors::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		TSharedPtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences = MakeShared<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, PointDataFacade->Source))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
			return false;
		}

		TSet<FSoftObjectPath> UniqueActorReferences;
		ActorReferences->GrabUniqueValues(UniqueActorReferences);

		MainThreadToken = AsyncManager->TryCreateToken(FName("DestroyActors"));
		if (!MainThreadToken.IsValid()) { return false; }

		Context->SourceComponent->ForEachManagedResource(
			[&](UPCGManagedResource* InResource)
			{
				UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(InResource);

				if (!ManagedActors || ManagedActors->GeneratedActors.IsEmpty()) { return; }

				TSet<TSoftObjectPtr<AActor>> ItCopy = ManagedActors->GeneratedActors;

				for (const TSoftObjectPtr<AActor>& Actor : ItCopy)
				{
					if (UniqueActorReferences.Contains(Actor->GetPathName()))
					{
						ManagedActors->Release(false, ActorsToDelete);
						return;
					}
				}
			});

		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction(
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				for (const TSoftObjectPtr<AActor>& ActorRef : This->ActorsToDelete)
				{
					if (ActorRef.IsValid()) { ActorRef->Destroy(); }
				}

				PCGEX_ASYNC_RELEASE_TOKEN(This->MainThreadToken)
			});

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Utils/PCGExDestroyActor.h"

#include "PCGComponent.h"


#include "PCGExSubSystem.h"
#include "PCGManagedResource.h"
#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExDestroyActorElement"
#define PCGEX_NAMESPACE DestroyActor

UPCGExDestroyActorSettings::UPCGExDestroyActorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EIOInit UPCGExDestroyActorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(DestroyActor)
PCGEX_ELEMENT_BATCH_POINT_IMPL(DestroyActor)

FName UPCGExDestroyActorSettings::GetMainInputPin() const
{
	return PCGExCommon::Labels::SourceTargetsLabel;
}

bool FPCGExDestroyActorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DestroyActor)

	return true;
}

bool FPCGExDestroyActorElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDestroyActorElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DestroyActor)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExDestroyActor
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExDestroyActor::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		TSharedPtr<PCGExData::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences = MakeShared<PCGExData::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, PointDataFacade->Source))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
			return false;
		}

		TSet<FSoftObjectPath> UniqueActorReferences;
		ActorReferences->GrabUniqueValues(UniqueActorReferences);

		MainThreadToken = TaskManager->TryCreateToken(FName("DestroyActors"));
		if (!MainThreadToken.IsValid()) { return false; }

		Context->GetMutableComponent()->ForEachManagedResource([&](UPCGManagedResource* InResource)
		{
			UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(InResource);

			if (!ManagedActors) { return; }

			TArray<TSoftObjectPtr<AActor>> GeneratedActors = ManagedActors->GetConstGeneratedActors();

			if (!ManagedActors || GeneratedActors.IsEmpty()) { return; }

			for (const TSoftObjectPtr<AActor>& Actor : GeneratedActors)
			{
				if (UniqueActorReferences.Contains(Actor->GetPathName()))
				{
					ManagedActors->Release(false, ActorsToDelete);
					return;
				}
			}
		});

		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction([PCGEX_ASYNC_THIS_CAPTURE]()
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

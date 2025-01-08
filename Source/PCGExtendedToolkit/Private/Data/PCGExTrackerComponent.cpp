// Copyright (c) Nebukam


#include "Data/PCGExTrackerComponent.h"

#include "PCGExSubSystem.h"
#include "PCGSubsystem.h"

void UPCGExEventObserver::AddObserver(const TObjectPtr<UActorComponent>& InComponent)
{
	FWriteScopeLock WriteScopeLock(TrackingLock);
	Observers.Add(InComponent);
}

void UPCGExEventObserver::RemoveObserver(const TObjectPtr<UActorComponent>& InComponent)
{
	FWriteScopeLock WriteScopeLock(TrackingLock);
	Observers.Remove(InComponent);
}

UPCGExTrackerComponent::UPCGExTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPCGExTrackerComponent::StartTracking(const TObjectPtr<UPCGComponent>& InComponent)
{
	check(InComponent->GetOwner() == InComponent->GetOwner())

	bool bAlreadyTracked = false;
	TrackedComponents.Add(InComponent, &bAlreadyTracked);

	if (bAlreadyTracked) { return; }

	OnTrackingStarted.Broadcast(this, InComponent);
}

void UPCGExTrackerComponent::StopTracking(const TObjectPtr<UPCGComponent>& InComponent)
{
	if (TrackedComponents.Remove(InComponent) <= 0) { return; }
	OnTrackingEnded.Broadcast(this, InComponent);
}

void UPCGExTrackerComponent::StopTrackingAll()
{
	TArray<TSoftObjectPtr<UPCGComponent>> Components = TrackedComponents.Array();
	for (TSoftObjectPtr<UPCGComponent> C : Components)
	{
		if (C.Get()) { StopTracking(C.Get()); }
	}
}

void UPCGExTrackerComponent::BindForRegeneration(const TObjectPtr<UPCGComponent>& InComponent, const uint32 EventId)
{
	StartTracking(InComponent);

	TObjectPtr<UPCGExEventObserver>* Handler = EventObserverRegenerate.Find(EventId);

	if (!Handler)
	{
		TObjectPtr<UPCGExEventObserver> NewHandler;
		{
			FGCScopeGuard Scope;
			NewHandler = NewObject<UPCGExEventObserver>();
		}

		NewHandler->EventId = EventId;
		Handler = &EventObserverRegenerate.Add(EventId, NewHandler);

		if (NewHandler->HasAnyInternalFlags(EInternalObjectFlags::Async)) { NewHandler->ClearInternalFlags(EInternalObjectFlags::Async); }
	}

	Handler->Get()->AddObserver(InComponent);
}

void UPCGExTrackerComponent::BeginPlay()
{
	Super::BeginPlay();

	PCGEX_SUBSYSTEM
	PCGExSubsystem->OnGlobalEvent.AddDynamic(this, &UPCGExTrackerComponent::OnGlobalEvent);
}

void UPCGExTrackerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTrackingAll();

	Super::EndPlay(EndPlayReason);

	OnTrackingStarted.Clear();
	OnTrackingEnded.Clear();
}

void UPCGExTrackerComponent::OnGlobalEvent(UPCGComponent* Source, EPCGExSubsystemEventType EventType, uint32 EventId)
{
	const TObjectPtr<UPCGExEventObserver>* EventObserver = nullptr;

	// TODO : Need to find a more elegant way to handle more usecases in a generic way
	if (EventType == EPCGExSubsystemEventType::Regenerate)
	{
		EventObserver = EventObserverRegenerate.Find(EventId);
		if (!EventObserver) { return; }

		EventObserver->Get()->ForEachObserver<UPCGComponent>(
			[&](UPCGComponent* C)
			{
				if (C == Source) { return; } // Skip source

				// Cancel any in-progress generation
				if (C->IsGenerating()) { C->CancelGeneration(); }

				if (C->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand)
				{
					C->Generate(true);
					return;
				}

#if PCGEX_ENGINE_VERSION > 503
				if (C->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime)
				{
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
					{
						PCGSubsystem->RefreshRuntimeGenComponent(C, EPCGChangeType::GenerationGrid);
					}
				}
#endif
			});
	}
	else
	{
		//
	}
}

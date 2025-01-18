// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExSubSystem.h"

#include "PCGComponent.h"
#include "Data/PCGExDataSharing.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ObjectTools.h"
#else
#include "Engine/Engine.h"
#include "Engine/World.h"
#endif

UPCGExSubSystem::UPCGExSubSystem()
	: Super()
{
}

void UPCGExSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SharedDataManager = NewObject<UPCGExSharedDataManager>(this);
}

void UPCGExSubSystem::Deinitialize()
{
	Super::Deinitialize();
}

UPCGExSubSystem* UPCGExSubSystem::GetSubsystemForCurrentWorld()
{
	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->PlayWorld)
		{
			World = GEditor->PlayWorld;
		}
		else
		{
			World = GEditor->GetEditorWorldContext().World();
		}
	}
	else
#endif
		if (GEngine)
		{
			World = GEngine->GetCurrentPlayWorld();
		}

	return GetInstance(World);
}

void UPCGExSubSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ExecuteBeginTickActions();
}

ETickableTickType UPCGExSubSystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UPCGExSubSystem::IsTickable() const
{
	return bWantsTick;
}

TStatId UPCGExSubSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPCGExSubsystem, STATGROUP_Tickables);
}

UPCGExSubSystem* UPCGExSubSystem::GetInstance(UWorld* World)
{
	if (World)
	{
		return World->GetSubsystem<UPCGExSubSystem>();
	}
	return nullptr;
}

void UPCGExSubSystem::RegisterBeginTickAction(FTickAction&& Action)
{
	FWriteScopeLock WriteScopeLock(SubsystemLock);
	bWantsTick = true;
	BeginTickActions.Emplace(Action);
}

void UPCGExSubSystem::PollEvent(UPCGComponent* InSource, const EPCGExSubsystemEventType InEventType, const uint32 InEventId)
{
	FWriteScopeLock WriteScopeLock(SubsystemLock);
	bWantsTick = true;
	PolledEvents.Add(PCGEx::FPolledEvent(InSource, InEventType, InEventId));
}

void UPCGExSubSystem::ExecuteBeginTickActions()
{
	TArray<FTickAction> Actions;
	TArray<PCGEx::FPolledEvent> Events;

	{
		FWriteScopeLock WriteScopeLock(SubsystemLock);
		bWantsTick = false;

		Actions = MoveTemp(BeginTickActions);
		BeginTickActions.Reset();

		Events = PolledEvents.Array();
		PolledEvents.Reset();
	}

	for (const PCGEx::FPolledEvent& Event : Events) { OnGlobalEvent.Broadcast(Event.Source, Event.Type, Event.EventId); }
	for (FTickAction& Action : Actions) { Action(); }
}

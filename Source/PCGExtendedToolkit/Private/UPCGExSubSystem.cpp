// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "UPCGExSubSystem.h"

#include "PCGComponent.h"

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

	return UPCGExSubSystem::GetInstance(World);
}

void UPCGExSubSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ExecuteBeginTickActions();
}

ETickableTickType UPCGExSubSystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always;
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
	else
	{
		return nullptr;
	}
}

void UPCGExSubSystem::RegisterBeginTickAction(FTickAction&& Action)
{
	FWriteScopeLock WriteScopeLock(TickActionsLock);
	BeginTickActions.Emplace(Action);
}

void UPCGExSubSystem::ExecuteBeginTickActions()
{
	TArray<FTickAction> Actions;
	
	{
		FWriteScopeLock WriteScopeLock(TickActionsLock);
		Actions = MoveTemp(BeginTickActions);
		BeginTickActions.Reset();
	}

	for (FTickAction& Action : Actions) { Action(); }
}

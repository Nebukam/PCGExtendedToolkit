// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExSubSystem.h"

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
	else
	{
		return nullptr;
	}
}

void UPCGExSubSystem::RegisterBeginTickAction(FTickAction&& Action)
{
	FWriteScopeLock WriteScopeLock(TickActionsLock);
	bWantsTick = true;
	BeginTickActions.Emplace(Action);
}

void UPCGExSubSystem::RegisterGlobalData(FName InName, const TObjectPtr<UPCGData>& InData, const TSet<FString>& InTags)
{
}

void UPCGExSubSystem::UnregisterGlobalData(const TObjectPtr<UPCGData>& InData)
{
}

UPCGExDataHolder* UPCGExSubSystem::FindGlobalDataByKey(uint32 Key)
{
	FReadScopeLock ReadScopeLock(GlobalPCGDataLock);
	if (TObjectPtr<UPCGExDataHolder>* DataHolder = GlobalPCGData.Find(Key)) { return *DataHolder; }
	return nullptr;
}

UPCGExDataHolder* UPCGExSubSystem::FindGlobalDataById(const FName InId, const EPCGDataType InTypeFilter)
{
	FReadScopeLock ReadScopeLock(GlobalPCGDataLock);

	for (const TPair<uint32, TObjectPtr<UPCGExDataHolder>>& Pair : GlobalPCGData)
	{
		if (Pair.Value->Id == InId && !!(InTypeFilter & EPCGDataType::Point)) { return Pair.Value; }
	}
	
	return nullptr;
}

void UPCGExSubSystem::FindTaggedGlobalData(const FString& Tagged, const TSet<FString>& NotTagged)
{
}

void UPCGExSubSystem::ExecuteBeginTickActions()
{
	TArray<FTickAction> Actions;

	{
		FWriteScopeLock WriteScopeLock(TickActionsLock);
		bWantsTick = false;
		Actions = MoveTemp(BeginTickActions);
		BeginTickActions.Reset();
	}

	for (FTickAction& Action : Actions) { Action(); }
}

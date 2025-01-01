// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Subsystems/WorldSubsystem.h"

#include "UPCGExSubSystem.generated.h"

#define PCGEX_SUBSYSTEM UPCGExSubSystem* PCGExSubsystem = UPCGExSubSystem::GetSubsystemForCurrentWorld(); check(PCGExSubsystem)

UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

private:
	FRWLock TickActionsLock;

public:
	UPCGExSubSystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** To be used when a PCG component can not have a world anymore, to unregister itself. */
	static UPCGExSubSystem* GetSubsystemForCurrentWorld();

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** Will return the subsystem from the World if it exists and if it is initialized */
	static UPCGExSubSystem* GetInstance(UWorld* World);

	/** Adds an action that will be executed once at the beginning of this subsystem's next Tick(). */
	using FTickAction = TFunction<void()>;
	void RegisterBeginTickAction(FTickAction&& Action);

protected:
	/** Functions will be executed at the beginning of the tick and then removed from this array. */
	TArray<FTickAction> BeginTickActions;

	void ExecuteBeginTickActions();
};

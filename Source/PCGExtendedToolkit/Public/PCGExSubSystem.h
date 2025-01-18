// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataFilter.h"
#include "Engine/Level.h"
#include "Subsystems/WorldSubsystem.h"
#include "PCGData.h"

#include "PCGExSubSystem.generated.h"

#define PCGEX_SUBSYSTEM UPCGExSubSystem* PCGExSubsystem = UPCGExSubSystem::GetSubsystemForCurrentWorld(); check(PCGExSubsystem)

UENUM()
enum class EPCGExSubsystemEventType : uint8
{
	None       = 0 UMETA(Hidden),
	Regenerate = 1 UMETA(DisplayName = "Regenerate", Tooltip="Triggers regeneration on subcribers."),
	DataUpdate = 2 UMETA(DisplayName = "Data Update", Tooltip="Triggers a data update event."),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGlobalEvent, UPCGComponent*, Source, EPCGExSubsystemEventType, EventType, uint32, EventId);

class UPCGExSharedDataManager;

namespace PCGEx
{
	struct FPolledEvent
	{
		UPCGComponent* Source = nullptr;
		EPCGExSubsystemEventType Type = EPCGExSubsystemEventType::None;
		uint32 EventId = 0;

		FPolledEvent() = default;

		FPolledEvent(UPCGComponent* InSource, const EPCGExSubsystemEventType InType, const uint32 InEventId)
			: Source(InSource), Type(InType), EventId(InEventId)
		{
		}

		~FPolledEvent() = default;

		bool operator==(const FPolledEvent& Other) const { return Source == Other.Source && Type == Other.Type && EventId == Other.EventId; }
		FORCEINLINE friend uint32 GetTypeHash(const FPolledEvent& Key) { return HashCombineFast(static_cast<uint32>(Key.Type), HashCombineFast(GetTypeHash(Key.Source), Key.EventId)); }
	};
}

UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExSubSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	FRWLock SubsystemLock;

public:
	UPCGExSubSystem();

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnGlobalEvent OnGlobalEvent;

	UPROPERTY(Transient)
	TObjectPtr<UPCGExSharedDataManager> SharedDataManager;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** To be used when a PCG component can not have a world anymore, to unregister itself. */
	static UPCGExSubSystem* GetSubsystemForCurrentWorld();

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** Will return the subsystem from the World if it exists and if it is initialized */
	static UPCGExSubSystem* GetInstance(UWorld* World);

	/** Adds an action that will be executed once at the beginning of this subsystem's next Tick(). */
	using FTickAction = TFunction<void()>;
	void RegisterBeginTickAction(FTickAction&& Action);

	void PollEvent(UPCGComponent* InSource, EPCGExSubsystemEventType InEventType, uint32 InEventId);

protected:
	bool bWantsTick = false;

	/** Functions will be executed at the beginning of the tick and then removed from this array. */

	TArray<FTickAction> BeginTickActions;
	TSet<PCGEx::FPolledEvent> PolledEvents;

	void ExecuteBeginTickActions();
};

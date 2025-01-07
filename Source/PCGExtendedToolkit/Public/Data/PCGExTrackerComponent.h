// Copyright (c) Nebukam

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"
#include "PCGExSubSystem.h"
#include "Components/ActorComponent.h"
#include "PCGExTrackerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrackingStarted, UPCGExTrackerComponent*, Tracker, UPCGComponent*, Component);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrackingEnded, UPCGExTrackerComponent*, Tracker, UPCGComponent*, Component);

UCLASS(Hidden)
class UPCGExEventObserver : public UObject
{
	GENERATED_BODY()

	mutable FRWLock TrackingLock;

public:
	UPROPERTY()
	uint32 EventId;

	UPROPERTY()
	TSet<TSoftObjectPtr<UActorComponent>> Observers;

	void AddObserver(const TObjectPtr<UActorComponent>& InComponent);
	void RemoveObserver(const TObjectPtr<UActorComponent>& InComponent);

	template <typename T>
	void ForEachObserver(const TFunction<void(T*)>& Func)
	{
		for (TArray<TSoftObjectPtr<UActorComponent>> A = Observers.Array();
		     const TSoftObjectPtr<UActorComponent>& C : A)
		{
			if (T* CC = Cast<T>(C.Get())) { Func(CC); }
			else
			{
				// Cleanup dead references
				FWriteScopeLock WriteScopeLock(TrackingLock);
				Observers.Remove(C);
			}
		}
	}
};

UCLASS(Hidden, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PCGEXTENDEDTOOLKIT_API UPCGExTrackerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPCGExTrackerComponent();

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnTrackingStarted OnTrackingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnTrackingEnded OnTrackingEnded;

	void StartTracking(const TObjectPtr<UPCGComponent>& InComponent);
	void StopTracking(const TObjectPtr<UPCGComponent>& InComponent);

	void StopTrackingAll();

	void BindForRegeneration(const TObjectPtr<UPCGComponent>& InComponent, uint32 EventId);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
	TSet<TSoftObjectPtr<UPCGComponent>> TrackedComponents;

	UFUNCTION()
	virtual void OnGlobalEvent(UPCGComponent* Source, EPCGExSubsystemEventType EventType, uint32 EventId);

	UPROPERTY()
	TMap<uint32, TObjectPtr<UPCGExEventObserver>> EventObserverRegenerate;
};

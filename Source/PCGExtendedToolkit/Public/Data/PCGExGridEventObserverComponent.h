// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExGridTracking.h"
#include "Components/ActorComponent.h"
#include "PCGExGridEventObserverComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventCreated, int32, Count);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEventDiff, int32, Count, int32, Diff);

DECLARE_DELEGATE(FOnEventDestroyed);

UCLASS(Hidden, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PCGEXTENDEDTOOLKIT_API UPCGExGridEventObserverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPCGExGridEventObserverComponent();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExGridID GridID;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnEventCreated OnEventCreated;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnEventDiff OnEventDiff;

	//UPROPERTY(BlueprintAssignable, Category = "Delegates")
	//FOnEventDestroyed OnEventDestroyed;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool bObserving = false;
};

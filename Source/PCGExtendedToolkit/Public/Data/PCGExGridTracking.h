// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"

#include "PCGExGridTracking.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGridIDCreated, int32, Hash, int32, Count);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGridIDiff, int32, Hash, int32, Count, int32, Diff);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGridIDDestroyed, int32, Hash);

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGridID
{
	GENERATED_BODY()

	FPCGExGridID() = default;

	explicit FPCGExGridID(const FVector& InLocation, const int32 InGridSize = 25600, const FName InWorldID = NAME_None);
	FPCGExGridID(const UPCGComponent* InComponent, const FVector& InLocation, const FName InName = NAME_None);
	FPCGExGridID(const UPCGComponent* InComponent, const FName InName = NAME_None);

	/** Optional name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Name = NAME_None;

	/** Grid Size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1, UIMin=1))
	int32 GridSize = 25600;

	/** Arbitrary position, will be translated to grid indices */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector Location = FVector::ZeroVector;

	FPCGExGridID MakeFromGridID(const FVector& InLocation) const;

	operator uint32() const
	{
		return HashCombineFast(
			GetTypeHash(
				FIntVector(
						FMath::FloorToInt(Location.X / GridSize),
						FMath::FloorToInt(Location.Y / GridSize),
						FMath::FloorToInt(Location.Z / GridSize)
					)),
			GetTypeHash(Name));
	}

	FORCEINLINE friend uint32 GetTypeHash(const FPCGExGridID& GridID) { return GridID; }
};

namespace PCGExGridTracking
{
	using FEventCreatedCallback = std::function<void(const uint32)>;           // <EventID, Counter>
	using FEventDiffCallback = std::function<void(const uint32, const int32)>; // <EventID, Diff>
	using FEventDestroyedCallback = std::function<void(const uint32)>;         // <EventID>
	//FEventCallback OnIterationCallback;
}

UCLASS(Hidden)
class UPCGExGridIDTracker : public UObject
{
	GENERATED_BODY()

	mutable FRWLock BucketLock;
	mutable FRWLock PollLock;

public:
	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnGridIDCreated OnGridIDCreated;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnGridIDiff OnGridIDDiff;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnGridIDDestroyed OnGridIDDestroyed;

	int32 GetCounter(uint32 Hash) const;

	void PollEvent(uint32 Hash, const int32 Diff = 1);

protected:
	std::atomic<bool> bIsTickScheduled{false};

	TMap<uint32, int32> Buckets;      // <Event, Counter>
	TMap<uint32, int32> PolledEvents; // <Event, Diff>

	void ProcessPolledEvents();
};

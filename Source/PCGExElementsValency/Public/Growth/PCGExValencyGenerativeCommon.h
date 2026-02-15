// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyCommon.h"

#include "PCGExValencyGenerativeCommon.generated.h"

/**
 * Tracking a placed module during generative growth.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExPlacedModule
{
	GENERATED_BODY()

	/** Index into compiled modules */
	int32 ModuleIndex = -1;

	/** Final world-space transform */
	FTransform WorldTransform = FTransform::Identity;

	/** Transformed + modified bounds in world space */
	FBox WorldBounds = FBox(ForceInit);

	/** Index in PlacedModules of parent (-1 for seeds) */
	int32 ParentIndex = -1;

	/** Which connector on parent this attached to */
	int32 ParentConnectorIndex = -1;

	/** Which connector on this module was used for attachment */
	int32 ChildConnectorIndex = -1;

	/** Distance from seed (0 = seed itself) */
	int32 Depth = 0;

	/** Which seed spawned this chain */
	int32 SeedIndex = -1;

	/** Sum of module weights from seed to here */
	float CumulativeWeight = 0.0f;
};

/**
 * An open connector on the growth frontier - a candidate for further expansion.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExOpenConnector
{
	GENERATED_BODY()

	/** Index in PlacedModules array */
	int32 PlacedModuleIndex = -1;

	/** Index into module's connectors array */
	int32 ConnectorIndex = -1;

	/** Cached connector type for fast compatibility lookup */
	FName ConnectorType;

	/** Cached connector polarity for compatibility check */
	EPCGExConnectorPolarity Polarity = EPCGExConnectorPolarity::Universal;

	/** Pre-computed world-space connector transform */
	FTransform WorldTransform = FTransform::Identity;

	/** Depth of the owning module */
	int32 Depth = 0;

	/** Weight budget consumed so far */
	float CumulativeWeight = 0.0f;
};

/**
 * Growth budget controlling expansion limits.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExGrowthBudget
{
	GENERATED_BODY()

	/** Hard cap on total placed modules */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Budget", meta = (ClampMin = "1"))
	int32 MaxTotalModules = 100;

	/** Max distance from any seed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Budget", meta = (ClampMin = "1"))
	int32 MaxDepth = 10;

	/** Cumulative module weight budget per seed (-1 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Budget")
	float MaxWeightPerSeed = -1.0f;

	/** Stop a branch if no placement found for a socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Budget")
	bool bStopOnFirstFailure = false;

	/** Maximum candidate transforms to attempt per connector (constraint budget) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Budget", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxCandidatesPerConnector = 16;

	/** Runtime: current total placed count */
	int32 CurrentTotal = 0;

	bool CanPlaceMore() const { return CurrentTotal < MaxTotalModules; }
	bool CanGrowDeeper(int32 CurrentDepth) const { return CurrentDepth < MaxDepth; }

	bool CanAfford(float CurrentCumulativeWeight, float ModuleWeight) const
	{
		if (MaxWeightPerSeed < 0.0f) { return true; }
		return (CurrentCumulativeWeight + ModuleWeight) <= MaxWeightPerSeed;
	}

	void Reset() { CurrentTotal = 0; }
};

/**
 * Spatial occupancy tracker for preventing module overlap during growth.
 * Uses linear scan for simplicity (growth is sequential, typically <1000 modules).
 */
class PCGEXELEMENTSVALENCY_API FPCGExBoundsTracker
{
public:
	/** Check if a candidate box overlaps any existing placement */
	bool Overlaps(const FBox& Candidate) const;

	/** Register a newly placed module's bounds */
	void Add(const FBox& Bounds);

	/** Reset all tracked bounds */
	void Reset();

	/** Get current count of tracked bounds */
	int32 Num() const { return OccupiedBounds.Num(); }

private:
	TArray<FBox> OccupiedBounds;
};

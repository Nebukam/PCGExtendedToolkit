// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsCluster.generated.h"


UENUM()
enum class EPCGExClusterClosestSearchMode : uint8
{
	Vtx  = 0 UMETA(DisplayName = "Closest vtx", ToolTip="Proximity to node position"),
	Edge = 1 UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
};

UENUM()
enum class EPCGExClusterElement : uint8
{
	Vtx  = 0 UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated.", ActionIcon="Vtx"),
	Edge = 1 UMETA(DisplayName = "Edge", Tooltip="Value is fetched from the edge connecting to the point being evaluated.", ActionIcon="Edges"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNodeSelectionDetails
{
	GENERATED_BODY()

	FPCGExNodeSelectionDetails()
	{
	}

	explicit FPCGExNodeSelectionDetails(const double InMaxDistance):
		MaxDistance(InMaxDistance)
	{
	}

	~FPCGExNodeSelectionDetails()
	{
	}

	/** Drives how the seed & goal points are selected within each cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterClosestSearchMode PickingMethod = EPCGExClusterClosestSearchMode::Edge;

	/** Max distance at which a node can be selected. Use <= 0 to ignore distance check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1))
	double MaxDistance = -1;

	// TODO : Support local attribute

	FORCEINLINE bool WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const
	{
		if (MaxDistance <= 0) { return true; }
		return FVector::Distance(NodePosition, TargetPosition) < MaxDistance;
	}
};

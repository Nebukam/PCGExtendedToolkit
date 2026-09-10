// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExDataCacheHelpers.generated.h"

class AActor;
struct FPCGExContext;

UENUM()
enum class EPCGExDataCacheTarget : uint8
{
	ExecutingActor = 0 UMETA(DisplayName = "Executing Actor", Tooltip = "The actor that owns the executing PCG component. With partitioning, this is the partition actor of the current grid cell."),
	OriginalActor  = 1 UMETA(DisplayName = "Original Actor", Tooltip = "The actor that owns the original (non-partitioned) PCG component. Same as Executing Actor when not partitioned."),
	WorldActor     = 2 UMETA(DisplayName = "PCG World Actor", Tooltip = "The level's PCG World Actor, shared by every component in the world."),
};

namespace PCGExDataCache
{
	const FName TargetActorPinLabel = TEXT("Target Actor");
	const FName StatusPinLabel = TEXT("Status");

	const FName FoundAttributeName = TEXT("Found");
	const FName EntryCountAttributeName = TEXT("EntryCount");
	const FName CacheIDAttributeName = TEXT("CacheID");
	const FString CacheIDTagPrefix = TEXT("CacheID:");

	/**
	 * Game thread only (resolves soft paths, may spawn the PCG World Actor). When the Target Actor pin carries
	 * data, every unique actor referenced by InActorReferenceAttribute is a target (a component reference resolves
	 * to its owner); otherwise the single actor named by InTarget.
	 */
	PCGEXELEMENTSBRIDGES_API void ResolveTargetActors(
		FPCGExContext* InContext,
		const EPCGExDataCacheTarget InTarget,
		const FPCGAttributePropertyInputSelector& InActorReferenceAttribute,
		TArray<AActor*>& OutActors);
}

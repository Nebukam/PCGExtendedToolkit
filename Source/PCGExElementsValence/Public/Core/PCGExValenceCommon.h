// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValenceCommon.generated.h"

namespace PCGExValence
{
	/** Algorithm state constants */
	namespace SlotState
	{
		constexpr int32 UNSET = -1;        // Not yet resolved
		constexpr int32 NULL_SLOT = -2;    // Boundary / no neighbor exists
		constexpr int32 UNSOLVABLE = -3;   // Contradiction detected
		constexpr int32 PLACEHOLDER = -4;  // For ligature replacement
	}

	/** Pin labels */
	namespace Labels
	{
		const FName SourceRulesetLabel = FName("Ruleset");
		const FName SourceSolverLabel = FName("Solver");
		const FName SourceClustersLabel = FName("Clusters");
		const FName OutputStagedLabel = FName("Staged");
	}
}


/**
 * Wrapper for array of module indices (needed for TMap UPROPERTY support).
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceNeighborIndices
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neighbors")
	TArray<int32> Indices;

	void Add(int32 Index) { Indices.Add(Index); }
	void AddUnique(int32 Index) { Indices.AddUnique(Index); }
	int32 Num() const { return Indices.Num(); }
	bool Contains(int32 Index) const { return Indices.Contains(Index); }
};

/**
 * Per-layer socket configuration for a module.
 * Stores which sockets this module has and which neighbors are valid for each socket.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceModuleLayerConfig
{
	GENERATED_BODY()

	/** Bitmask indicating which sockets this module has (bits set = socket exists) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 SocketMask = 0;

	/** Valid neighbor module indices per socket. Key = SocketName, Value = array of valid module indices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TMap<FName, FPCGExValenceNeighborIndices> SocketNeighbors;

	/** Check if this module has a specific socket */
	bool HasSocket(int32 BitIndex) const
	{
		return (SocketMask & (1LL << BitIndex)) != 0;
	}

	/** Set a socket as present */
	void SetSocket(int32 BitIndex)
	{
		SocketMask |= (1LL << BitIndex);
	}

	/** Add a valid neighbor for a socket */
	void AddValidNeighbor(const FName& SocketName, int32 NeighborModuleIndex)
	{
		FPCGExValenceNeighborIndices& Neighbors = SocketNeighbors.FindOrAdd(SocketName);
		Neighbors.AddUnique(NeighborModuleIndex);
	}
};

/**
 * A module definition - represents one placeable asset with its socket configuration.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceModuleDefinition
{
	GENERATED_BODY()

	/** Unique index for this module (auto-assigned during compilation) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	int32 ModuleIndex = -1;

	/** The asset to spawn (mesh, actor, data asset, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TSoftObjectPtr<UObject> Asset;

	/** Probability weight for selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "0.001"))
	float Weight = 1.0f;

	/** Minimum number of times this module must be placed (0 = no minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Constraints", meta = (ClampMin = "0"))
	int32 MinSpawns = 0;

	/** Maximum number of times this module can be placed (-1 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Constraints", meta = (ClampMin = "-1"))
	int32 MaxSpawns = -1;

	/** Per-layer socket configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TMap<FName, FPCGExValenceModuleLayerConfig> Layers;

	/** Check if this module can still be spawned given current spawn count */
	bool CanSpawn(int32 CurrentSpawnCount) const
	{
		return MaxSpawns < 0 || CurrentSpawnCount < MaxSpawns;
	}

	/** Check if this module needs more spawns to meet minimum */
	bool NeedsMoreSpawns(int32 CurrentSpawnCount) const
	{
		return CurrentSpawnCount < MinSpawns;
	}
};

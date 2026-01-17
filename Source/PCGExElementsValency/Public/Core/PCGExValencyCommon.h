// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValencyCommon.generated.h"

namespace PCGExValency
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
		const FName SourceBondingRulesLabel = FName("BondingRules");
		const FName SourceSolverLabel = FName("Solver");
		const FName SourceClustersLabel = FName("Clusters");
		const FName OutputStagedLabel = FName("Staged");
	}
}


/**
 * Wrapper for array of module indices (needed for TMap UPROPERTY support).
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyNeighborIndices
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
 * Per-layer orbital configuration for a module.
 * Stores which orbitals this module has and which neighbors are valid for each orbital.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyModuleLayerConfig
{
	GENERATED_BODY()

	/** Bitmask indicating which orbitals this module has (bits set = orbital exists) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 OrbitalMask = 0;

	/** Valid neighbor module indices per orbital. Key = OrbitalName, Value = array of valid module indices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TMap<FName, FPCGExValencyNeighborIndices> OrbitalNeighbors;

	/** Check if this module has a specific orbital */
	bool HasOrbital(int32 BitIndex) const
	{
		return (OrbitalMask & (1LL << BitIndex)) != 0;
	}

	/** Set an orbital as present */
	void SetOrbital(int32 BitIndex)
	{
		OrbitalMask |= (1LL << BitIndex);
	}

	/** Add a valid neighbor for an orbital */
	void AddValidNeighbor(const FName& OrbitalName, int32 NeighborModuleIndex)
	{
		FPCGExValencyNeighborIndices& Neighbors = OrbitalNeighbors.FindOrAdd(OrbitalName);
		Neighbors.AddUnique(NeighborModuleIndex);
	}
};

/**
 * A module definition - represents one placeable asset with its orbital configuration.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyModuleDefinition
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

	/** Per-layer orbital configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TMap<FName, FPCGExValencyModuleLayerConfig> Layers;

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

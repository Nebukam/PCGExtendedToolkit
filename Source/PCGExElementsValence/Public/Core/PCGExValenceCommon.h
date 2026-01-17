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
 * Defines a single socket type within a layer.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceSocketDefinition
{
	GENERATED_BODY()

	/** Unique name for this socket type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName SocketName = NAME_None;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FText DisplayName;

	/** Optional direction vector for direction-based matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FVector Direction = FVector::ZeroVector;

	/** Debug visualization color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FLinearColor DebugColor = FLinearColor::White;

	/** Bit index in the layer's bitmask (0-63, auto-assigned during compilation) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Socket")
	int32 BitIndex = -1;
};

/**
 * A socket registry (layer) - defines a set of socket types.
 * Each layer can have up to 64 socket types.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceSocketRegistry
{
	GENERATED_BODY()

	/** Name of this layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FName LayerName = FName("Main");

	/** Socket definitions in this layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	TArray<FPCGExValenceSocketDefinition> Sockets;

	/** Get the bit index for a socket name, or -1 if not found */
	int32 GetSocketBitIndex(const FName& InSocketName) const
	{
		for (const FPCGExValenceSocketDefinition& Socket : Sockets)
		{
			if (Socket.SocketName == InSocketName)
			{
				return Socket.BitIndex;
			}
		}
		return -1;
	}

	/** Get socket count */
	int32 Num() const { return Sockets.Num(); }

	/** Validate and assign bit indices. Returns false if >64 sockets. */
	bool Compile()
	{
		if (Sockets.Num() > 64)
		{
			return false;
		}

		for (int32 i = 0; i < Sockets.Num(); ++i)
		{
			Sockets[i].BitIndex = i;
		}
		return true;
	}
};

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

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PCGExValenceCommon.h"
#include "PCGExValenceSocketCollection.h"

#include "PCGExValenceRuleset.generated.h"

/**
 * Compiled layer data optimized for runtime performance.
 * Uses flattened arrays for cache efficiency (similar to Unity Chemistry pattern).
 */
USTRUCT()
struct PCGEXELEMENTSVALENCE_API FPCGExValenceLayerCompiled
{
	GENERATED_BODY()

	/** Layer name */
	UPROPERTY()
	FName LayerName;

	/** Number of sockets in this layer */
	UPROPERTY()
	int32 SocketCount = 0;

	/**
	 * Per-module, per-socket neighbor data.
	 * Index = ModuleIndex * SocketCount + SocketIndex
	 * Value.X = Start index in AllNeighbors
	 * Value.Y = Count of valid neighbors
	 */
	UPROPERTY()
	TArray<FIntPoint> NeighborHeaders;

	/** Flattened array of all valid neighbor module indices */
	UPROPERTY()
	TArray<int32> AllNeighbors;

	/** Check if a module's socket accepts a specific neighbor */
	bool SocketAcceptsNeighbor(int32 ModuleIndex, int32 SocketIndex, int32 NeighborModuleIndex) const
	{
		const int32 HeaderIndex = ModuleIndex * SocketCount + SocketIndex;
		if (!NeighborHeaders.IsValidIndex(HeaderIndex))
		{
			return false;
		}

		const FIntPoint& Header = NeighborHeaders[HeaderIndex];
		const int32 Start = Header.X;
		const int32 End = Start + Header.Y;

		for (int32 i = Start; i < End; ++i)
		{
			if (AllNeighbors.IsValidIndex(i) && AllNeighbors[i] == NeighborModuleIndex)
			{
				return true;
			}
		}
		return false;
	}
};

/**
 * Compiled ruleset optimized for runtime solving.
 */
UCLASS()
class PCGEXELEMENTSVALENCE_API UPCGExValenceRulesetCompiled : public UObject
{
	GENERATED_BODY()

public:
	/** Total number of modules */
	UPROPERTY()
	int32 ModuleCount = 0;

	/** Module weights (parallel array) */
	UPROPERTY()
	TArray<float> ModuleWeights;

	/** Module socket masks per layer (Index = ModuleIndex * LayerCount + LayerIndex) */
	UPROPERTY()
	TArray<int64> ModuleSocketMasks;

	/** Module min spawn constraints */
	UPROPERTY()
	TArray<int32> ModuleMinSpawns;

	/** Module max spawn constraints (-1 = unlimited) */
	UPROPERTY()
	TArray<int32> ModuleMaxSpawns;

	/** Module asset references */
	UPROPERTY()
	TArray<TSoftObjectPtr<UObject>> ModuleAssets;

	/** Compiled layer data */
	UPROPERTY()
	TArray<FPCGExValenceLayerCompiled> Layers;

	/**
	 * Fast lookup: SocketMask -> array of candidate module indices.
	 * Key is the combined mask from all layers (for single-layer, just the mask).
	 * This allows O(1) lookup of which modules could potentially fit a node.
	 */
	TMap<int64, TArray<int32>> MaskToCandidates;

	/** Get layer count */
	int32 GetLayerCount() const { return Layers.Num(); }

	/** Get a module's socket mask for a specific layer */
	int64 GetModuleSocketMask(int32 ModuleIndex, int32 LayerIndex) const
	{
		const int32 Index = ModuleIndex * Layers.Num() + LayerIndex;
		return ModuleSocketMasks.IsValidIndex(Index) ? ModuleSocketMasks[Index] : 0;
	}

	/** Build the MaskToCandidates lookup table */
	void BuildCandidateLookup();
};

/**
 * Main ruleset data asset - the user-facing configuration.
 * Contains socket collection references and module definitions.
 */
UCLASS(BlueprintType)
class PCGEXELEMENTSVALENCE_API UPCGExValenceRuleset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Socket collections defining the layers. Each collection defines sockets for one layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valence|Layers")
	TArray<TSoftObjectPtr<UPCGExValenceSocketCollection>> SocketCollections;

	/** Module definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valence|Modules")
	TArray<FPCGExValenceModuleDefinition> Modules;

	/** Compiled runtime data (generated, not user-editable) */
	UPROPERTY()
	TObjectPtr<UPCGExValenceRulesetCompiled> CompiledData;

	/** Loaded socket collections (populated during Compile) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPCGExValenceSocketCollection>> LoadedSocketCollections;

	/** Get layer count */
	int32 GetLayerCount() const { return SocketCollections.Num(); }

	/** Get module count */
	int32 GetModuleCount() const { return Modules.Num(); }

	/** Find a socket collection by layer name */
	const UPCGExValenceSocketCollection* FindSocketCollection(const FName& LayerName) const
	{
		for (const TObjectPtr<UPCGExValenceSocketCollection>& Collection : LoadedSocketCollections)
		{
			if (Collection && Collection->LayerName == LayerName)
			{
				return Collection;
			}
		}
		return nullptr;
	}

	/** Find a module by asset */
	FPCGExValenceModuleDefinition* FindModuleByAsset(const TSoftObjectPtr<UObject>& Asset)
	{
		for (FPCGExValenceModuleDefinition& Module : Modules)
		{
			if (Module.Asset == Asset)
			{
				return &Module;
			}
		}
		return nullptr;
	}

	/** Get or create a module for an asset */
	FPCGExValenceModuleDefinition& GetOrCreateModule(const TSoftObjectPtr<UObject>& Asset)
	{
		if (FPCGExValenceModuleDefinition* Existing = FindModuleByAsset(Asset))
		{
			return *Existing;
		}

		FPCGExValenceModuleDefinition& NewModule = Modules.AddDefaulted_GetRef();
		NewModule.Asset = Asset;
		return NewModule;
	}

	/**
	 * Compile the ruleset for runtime use.
	 * Loads socket collections, validates layers, assigns module indices, and builds optimized lookup structures.
	 * @return True if compilation succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Valence")
	bool Compile();

	/** Check if the ruleset has valid compiled data */
	UFUNCTION(BlueprintCallable, Category = "Valence")
	bool IsCompiled() const { return CompiledData != nullptr; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

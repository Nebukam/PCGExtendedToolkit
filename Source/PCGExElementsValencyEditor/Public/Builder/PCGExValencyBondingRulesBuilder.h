// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/PCGExValencyBondingRules.h"

#include "PCGExValencyBondingRulesBuilder.generated.h"

class APCGExValencyCage;
class APCGExValencyCageBase;
class AValencyContextVolume;

/**
 * Result of a build operation.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyBuildResult
{
	GENERATED_BODY()

	/** Whether the build succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	/** Number of modules created/updated */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 ModuleCount = 0;

	/** Number of cages processed */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 CageCount = 0;

	/** Warning messages */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FText> Warnings;

	/** Error messages */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FText> Errors;
};

/**
 * Intermediate cage data used during building.
 */
struct FPCGExValencyCageData
{
	/** The source cage */
	TWeakObjectPtr<APCGExValencyCage> Cage;

	/** Assets registered on this cage */
	TArray<TSoftObjectPtr<UObject>> Assets;

	/** Orbital mask (which orbitals are connected) */
	int64 OrbitalMask = 0;

	/** Per-orbital: module indices of valid neighbors (from connected cage's assets) */
	TMap<int32, TArray<int32>> OrbitalToNeighborModules;
};

/**
 * Builds BondingRules from cages placed in a level.
 *
 * Usage:
 * 1. Create builder instance
 * 2. Call BuildFromVolume() with target volume
 * 3. Check result for success/warnings/errors
 */
UCLASS(BlueprintType)
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExValencyBondingRulesBuilder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Build bonding rules from all cages within a volume.
	 * Updates the volume's BondingRules asset in place.
	 *
	 * @param Volume The volume containing cages
	 * @return Build result with success status and any warnings/errors
	 */
	UFUNCTION(BlueprintCallable, Category = "Valency")
	FPCGExValencyBuildResult BuildFromVolume(AValencyContextVolume* Volume);

	/**
	 * Build bonding rules from a specific set of cages.
	 *
	 * @param Cages Array of cages to process
	 * @param TargetRules The BondingRules asset to update
	 * @param OrbitalSet The orbital set to use for direction matching
	 * @return Build result with success status and any warnings/errors
	 */
	UFUNCTION(BlueprintCallable, Category = "Valency")
	FPCGExValencyBuildResult BuildFromCages(
		const TArray<APCGExValencyCage*>& Cages,
		UPCGExValencyBondingRules* TargetRules,
		UPCGExValencyOrbitalSet* OrbitalSet
	);

	/** If true, clears existing modules before building (default: true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Options")
	bool bClearExistingModules = true;

	/** If true, validates completeness after building */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Options")
	bool bValidateCompleteness = true;

	/** If true, includes null cage connections as valid boundaries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Options")
	bool bIncludeNullBoundaries = true;

protected:
	/**
	 * Collect and preprocess cage data.
	 * Resolves mirror sources, computes orbital masks, etc.
	 */
	void CollectCageData(
		const TArray<APCGExValencyCage*>& Cages,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		TArray<FPCGExValencyCageData>& OutCageData,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Build asset-to-module index mapping.
	 * Creates modules for each unique asset.
	 */
	void BuildAssetToModuleMap(
		const TArray<FPCGExValencyCageData>& CageData,
		UPCGExValencyBondingRules* TargetRules,
		TMap<FSoftObjectPath, int32>& OutAssetToModule,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Populate neighbor relationships for each module.
	 */
	void BuildNeighborRelationships(
		const TArray<FPCGExValencyCageData>& CageData,
		const TMap<FSoftObjectPath, int32>& AssetToModule,
		UPCGExValencyBondingRules* TargetRules,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Validate the built rules for completeness.
	 */
	void ValidateRules(
		UPCGExValencyBondingRules* Rules,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Get effective assets for a cage (resolving mirror sources).
	 */
	TArray<TSoftObjectPtr<UObject>> GetEffectiveAssets(const APCGExValencyCage* Cage);
};

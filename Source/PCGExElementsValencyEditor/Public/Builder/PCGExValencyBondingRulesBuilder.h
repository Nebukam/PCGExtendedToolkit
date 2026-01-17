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

	/** Asset entries registered on this cage (with optional local transforms) */
	TArray<FPCGExValencyAssetEntry> AssetEntries;

	/** Orbital mask (which orbitals are connected) */
	int64 OrbitalMask = 0;

	/** Module settings from the cage */
	FPCGExValencyModuleSettings Settings;

	/** Whether this cage preserves local transforms */
	bool bPreserveLocalTransforms = false;

	/** Per-orbital: module indices of valid neighbors (from connected cage's assets) */
	TMap<int32, TArray<int32>> OrbitalToNeighborModules;

	/**
	 * Get a unique key for module lookup.
	 * When local transforms are preserved, includes transform hash for unique variants.
	 */
	static FString MakeModuleKey(const FSoftObjectPath& AssetPath, int64 Mask, const FTransform* LocalTransform = nullptr)
	{
		if (LocalTransform && !LocalTransform->Equals(FTransform::Identity, 0.01f))
		{
			// Include transform hash for unique variants
			const FVector Loc = LocalTransform->GetLocation();
			const FRotator Rot = LocalTransform->Rotator();
			return FString::Printf(TEXT("%s_%lld_%.0f%.0f%.0f_%.0f%.0f%.0f"),
				*AssetPath.ToString(), Mask,
				Loc.X, Loc.Y, Loc.Z,
				Rot.Pitch, Rot.Yaw, Rot.Roll);
		}
		return FString::Printf(TEXT("%s_%lld"), *AssetPath.ToString(), Mask);
	}
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
	 * Build module key to module index mapping.
	 * Creates modules for each unique Asset + OrbitalMask combination.
	 * Key format: "AssetPath_OrbitalMask"
	 */
	void BuildModuleMap(
		const TArray<FPCGExValencyCageData>& CageData,
		UPCGExValencyBondingRules* TargetRules,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		TMap<FString, int32>& OutModuleKeyToIndex,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Populate neighbor relationships for each module.
	 */
	void BuildNeighborRelationships(
		const TArray<FPCGExValencyCageData>& CageData,
		const TMap<FString, int32>& ModuleKeyToIndex,
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
	 * Get effective asset entries for a cage (resolving mirror sources).
	 */
	TArray<FPCGExValencyAssetEntry> GetEffectiveAssetEntries(const APCGExValencyCage* Cage);

	/**
	 * Generate a variant name for a module based on its asset and configuration.
	 */
	static FString GenerateVariantName(const FPCGExValencyAssetEntry& Entry, int64 OrbitalMask, bool bHasLocalTransform);
};

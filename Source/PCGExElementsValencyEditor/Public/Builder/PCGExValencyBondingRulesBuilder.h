// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "Core/PCGExValencyBondingRules.h"

#include "PCGExValencyBondingRulesBuilder.generated.h"

class APCGExValencyCage;
class APCGExValencyCageBase;
class APCGExValencyCagePattern;
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

	/** Number of patterns compiled */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 PatternCount = 0;

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

	/** Module name from the cage (for fixed picks) */
	FName ModuleName;

	/** Whether this cage preserves local transforms */
	bool bPreserveLocalTransforms = false;

	/** Compiled properties from cage property components */
	TArray<FInstancedStruct> Properties;

	/** Per-orbital: module indices of valid neighbors (from connected cage's assets) */
	TMap<int32, TArray<int32>> OrbitalToNeighborModules;

	/**
	 * Get a unique key for module lookup.
	 * Includes transform hash when transforms are preserved, and material variant hash when materials differ.
	 */
	static FString MakeModuleKey(const FSoftObjectPath& AssetPath, int64 Mask, const FTransform* LocalTransform = nullptr, const FPCGExValencyMaterialVariant* MaterialVariant = nullptr)
	{
		FString Key = FString::Printf(TEXT("%s_%lld"), *AssetPath.ToString(), Mask);

		// Include transform hash for unique variants
		if (LocalTransform && !LocalTransform->Equals(FTransform::Identity, 0.01f))
		{
			const FVector Loc = LocalTransform->GetLocation();
			const FRotator Rot = LocalTransform->Rotator();
			Key += FString::Printf(TEXT("_T%.0f%.0f%.0f_%.0f%.0f%.0f"),
				Loc.X, Loc.Y, Loc.Z,
				Rot.Pitch, Rot.Yaw, Rot.Roll);
		}

		// Include material variant hash - each unique material configuration is a separate module
		if (MaterialVariant && MaterialVariant->Overrides.Num() > 0)
		{
			Key += TEXT("_M");
			for (const FPCGExValencyMaterialOverride& Override : MaterialVariant->Overrides)
			{
				// Use material path hash for uniqueness
				const uint32 MatHash = Override.Material.IsValid()
					? GetTypeHash(Override.Material.ToSoftObjectPath().ToString())
					: 0;
				Key += FString::Printf(TEXT("%d:%u"), Override.SlotIndex, MatHash);
			}
		}

		return Key;
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
	 * Build bonding rules from all cages within multiple volumes.
	 * All volumes must reference the same BondingRules asset.
	 * Updates the shared BondingRules asset in place.
	 *
	 * @param Volumes Array of volumes containing cages (must share same BondingRules)
	 * @param TriggeringVolume The volume that initiated the build (for metadata)
	 * @return Build result with success status and any warnings/errors
	 */
	UFUNCTION(BlueprintCallable, Category = "Valency")
	FPCGExValencyBuildResult BuildFromVolumes(const TArray<AValencyContextVolume*>& Volumes, AValencyContextVolume* TriggeringVolume = nullptr);

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
	 * Validate that property names map to consistent types across all modules.
	 * Same property name with different types across cages = error.
	 */
	void ValidateModulePropertyTypes(
		UPCGExValencyBondingRules* TargetRules,
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
	 * Collect properties from a cage and its mirror sources.
	 * Properties from palettes and mirrored cages are inherited.
	 */
	TArray<FInstancedStruct> GetEffectiveProperties(const APCGExValencyCage* Cage);

	/**
	 * Generate a variant name for a module based on its asset and configuration.
	 */
	static FString GenerateVariantName(const FPCGExValencyAssetEntry& Entry, int64 OrbitalMask, bool bHasLocalTransform);

	/**
	 * Collect material variants from all cages.
	 * Cages discover variants during scanning; this merges them into TargetRules.
	 * Stores results in TargetRules->DiscoveredMaterialVariants.
	 */
	void DiscoverMaterialVariants(
		const TArray<FPCGExValencyCageData>& CageData,
		UPCGExValencyBondingRules* TargetRules
	);

	/**
	 * Compile patterns from pattern cages.
	 * Pattern cages are discovered within the volumes, grouped by connectivity,
	 * and compiled into FPCGExValencyPatternSetCompiled.
	 *
	 * @param Volumes Array of volumes to search for pattern cages
	 * @param ModuleKeyToIndex Mapping from module key to compiled module index
	 * @param TargetRules The BondingRules asset to update with compiled patterns
	 * @param OrbitalSet The orbital set to use for orbital index lookup
	 * @param OutResult Build result for warnings/errors
	 */
	void CompilePatterns(
		const TArray<AValencyContextVolume*>& Volumes,
		const TMap<FString, int32>& ModuleKeyToIndex,
		UPCGExValencyBondingRules* TargetRules,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		FPCGExValencyBuildResult& OutResult
	);

	/**
	 * Compile a single pattern from its root cage.
	 * Traverses connected pattern cages and builds the compiled pattern.
	 *
	 * @param RootCage The pattern root cage
	 * @param ModuleKeyToIndex Mapping from module key to compiled module index
	 * @param TargetRules The BondingRules for module lookup
	 * @param OrbitalSet The orbital set for orbital index lookup
	 * @param OutPattern The compiled pattern
	 * @param OutResult Build result for warnings/errors
	 * @return true if compilation succeeded
	 */
	bool CompileSinglePattern(
		APCGExValencyCagePattern* RootCage,
		const TMap<FString, int32>& ModuleKeyToIndex,
		UPCGExValencyBondingRules* TargetRules,
		const UPCGExValencyOrbitalSet* OrbitalSet,
		FPCGExValencyPatternCompiled& OutPattern,
		FPCGExValencyBuildResult& OutResult
	);
};

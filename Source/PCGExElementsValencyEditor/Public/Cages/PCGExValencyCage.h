// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"
#include "Core/PCGExValencyCommon.h"

#include "PCGExValencyCage.generated.h"

class UStaticMeshComponent;
class APCGExValencyAssetPalette;

/**
 * Flags controlling which local transform components are preserved when registering assets.
 * Used when bPreserveLocalTransforms is enabled.
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPCGExLocalTransformFlags : uint8
{
	None = 0 UMETA(Hidden),
	/** Preserve local translation (position relative to cage center) */
	Translation = 1 << 0,
	/** Preserve local rotation (orientation relative to cage) */
	Rotation = 1 << 1,
	/** Preserve local scale */
	Scale = 1 << 2,

	// Common combinations
	All = Translation | Rotation | Scale UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EPCGExLocalTransformFlags);

/**
 * Abstract base for Valency cages that can register assets.
 * Subclasses must implement IsActorInside() for containment detection.
 *
 * Use APCGExValencyCageSimple for built-in shape-based containment,
 * or subclass this directly for custom containment logic.
 */
UCLASS(Abstract, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCage : public APCGExValencyCageBase
{
	GENERATED_BODY()

public:
	APCGExValencyCage();

	//~ Begin AActor Interface
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	//~ Begin APCGExValencyCageBase Interface
	virtual FString GetCageDisplayName() const override;
	virtual bool IsNullCage() const override { return false; }
	//~ End APCGExValencyCageBase Interface

	//~ Begin Containment Interface

	/**
	 * Check if an actor is inside this cage's detection bounds.
	 * Override in subclasses to implement custom containment logic.
	 * @param Actor The actor to test
	 * @return True if the actor is considered "inside" this cage
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Cage|Detection")
	bool IsActorInside(AActor* Actor) const;
	virtual bool IsActorInside_Implementation(AActor* Actor) const { return false; }

	/**
	 * Check if a world location is inside this cage's detection bounds.
	 * Override in subclasses to implement custom containment logic.
	 * @param WorldLocation The location to test
	 * @return True if the location is inside this cage
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Cage|Detection")
	bool ContainsPoint(const FVector& WorldLocation) const;
	virtual bool ContainsPoint_Implementation(const FVector& WorldLocation) const { return false; }

	//~ End Containment Interface

	/** Get all asset entries for this cage (combines manual + scanned) */
	TArray<FPCGExValencyAssetEntry> GetAllAssetEntries() const;

	/** Get manual asset entries only */
	const TArray<FPCGExValencyAssetEntry>& GetManualAssetEntries() const { return ManualAssetEntries; }

	/** Get scanned asset entries only */
	const TArray<FPCGExValencyAssetEntry>& GetScannedAssetEntries() const { return ScannedAssetEntries; }

	/** Get simple asset list (without transforms) for backward compatibility */
	TArray<TSoftObjectPtr<UObject>> GetRegisteredAssets() const;

	/** Manually register an asset (user-defined, persisted) */
	void RegisterManualAsset(const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor = nullptr);

	/** Unregister a manually added asset */
	void UnregisterManualAsset(const TSoftObjectPtr<UObject>& Asset);

	/** Clear manually registered assets */
	void ClearManualAssets();

	/** Clear scanned assets (auto-detected) */
	void ClearScannedAssets();

	/** Scan for assets within cage bounds and register them as scanned */
	void ScanAndRegisterContainedAssets();

public:
	/** Color for editor visualization (mirror connections, debug drawing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage")
	FLinearColor CageColor = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);

	/**
	 * Manually registered asset entries (user-defined via details panel).
	 * These are persisted and not affected by auto-scanning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Assets", meta = (TitleProperty = "Asset"))
	TArray<FPCGExValencyAssetEntry> ManualAssetEntries;

	/**
	 * Auto-scanned asset entries (transient, rebuilt by ScanAndRegisterContainedAssets).
	 * Populated when bAutoRegisterContainedAssets is enabled.
	 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Assets")
	TArray<FPCGExValencyAssetEntry> ScannedAssetEntries;

	/**
	 * Mirror sources - cages or asset palettes whose content this cage references.
	 * Assets from all sources are combined with this cage's orbital configuration.
	 * Supports both APCGExValencyCage and APCGExValencyAssetPalette actors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Mirror", meta = (AllowedClasses = "/Script/PCGExElementsValencyEditor.PCGExValencyCage, /Script/PCGExElementsValencyEditor.PCGExValencyAssetPalette"))
	TArray<TObjectPtr<AActor>> MirrorSources;

	/**
	 * When enabled, mirror sources are resolved recursively.
	 * If source A mirrors source B, assets from B are also included.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Mirror", meta=(PCGEX_ValencyRebuild))
	bool bRecursiveMirror = true;

	/**
	 * Whether to show ghost preview meshes when mirroring.
	 * Ghost meshes appear as translucent versions of the mirrored content.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Mirror")
	bool bShowMirrorGhostMeshes = true;

	/**
	 * Whether to automatically scan for and register contained assets.
	 * If false, assets must be manually registered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta=(PCGEX_ValencyRebuild))
	bool bAutoRegisterContainedAssets = true;

	/**
	 * When enabled, preserves the spatial relationship between assets and the cage center.
	 * Useful when asset placement within the cage matters (e.g., corner placement).
	 * Each unique Asset + LocalTransform combination becomes a separate module variant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta=(PCGEX_ValencyRebuild))
	bool bPreserveLocalTransforms = false;

	/**
	 * Which components of the local transform to preserve.
	 * Only used when bPreserveLocalTransforms is enabled.
	 * Default: All (Translation + Rotation + Scale).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (Bitmask, BitmaskEnum = "/Script/PCGExElementsValencyEditor.EPCGExLocalTransformFlags", EditCondition = "bPreserveLocalTransforms", PCGEX_ValencyRebuild))
	uint8 LocalTransformFlags = static_cast<uint8>(EPCGExLocalTransformFlags::All);

	/**
	 * Module settings applied to all assets in this cage.
	 * These settings are copied to module definitions when building rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Module")
	FPCGExValencyModuleSettings ModuleSettings;

	/**
	 * Optional name for modules created from this cage.
	 * Used for fixed picks - vertices with a matching FixedPick attribute value
	 * will be forced to use a module from this cage.
	 * Multiple cages can share the same name (selection uses weights).
	 * Empty = no name (cannot be fixed-picked by name).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Module", meta=(PCGEX_ValencyRebuild))
	FName ModuleName;

	/**
	 * Default behavior for orbitals without explicit connections.
	 * Applied during BuildNeighborRelationships when an orbital has no connected cage.
	 * - Unconstrained: No constraint (accepts any neighbor or none) - current default
	 * - Boundary: Treat as boundary (must have NO neighbor at that orbital)
	 * - Wildcard: Treat as wildcard (must have ANY neighbor at that orbital)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Module", AdvancedDisplay, meta = (PCGEX_ValencyRebuild))
	EPCGExMissingConnectionBehavior MissingConnectionBehavior = EPCGExMissingConnectionBehavior::Unconstrained;

	/**
	 * Material variants discovered during asset scanning.
	 * Key = mesh asset path, Value = array of unique material configurations.
	 * Populated by ScanAndRegisterContainedAssets, consumed by builder.
	 */
	TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>> DiscoveredMaterialVariants;

	/** Get discovered material variants for this cage */
	const TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>>& GetDiscoveredMaterialVariants() const
	{
		return DiscoveredMaterialVariants;
	}

	/** Check if a specific local transform component should be preserved */
	bool ShouldPreserveTranslation() const
	{
		return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Translation);
	}

	bool ShouldPreserveRotation() const
	{
		return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Rotation);
	}

	bool ShouldPreserveScale() const
	{
		return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Scale);
	}

	/** Compute the local transform to preserve based on flags */
	FTransform ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const;

	/**
	 * Rebuild ghost mesh components based on the mirror source's content.
	 * Called automatically when MirrorSource changes or when entering Valency mode.
	 */
	void RefreshMirrorGhostMeshes();

	/** Clear all ghost mesh components */
	void ClearMirrorGhostMeshes();

	/**
	 * Find all cages that have this cage in their MirrorSources array.
	 * Used to cascade updates when this cage's content changes.
	 */
	void FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const;

	/**
	 * Trigger rebuild for cages that mirror this cage.
	 * Called when this cage's MirrorSources changes to cascade the update.
	 * @return True if any rebuild was triggered
	 */
	bool TriggerAutoRebuildForMirroringCages();

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
#endif

protected:
	/** Called when asset registration changes */
	virtual void OnAssetRegistrationChanged();

	/**
	 * Check if scanned assets have changed compared to a previous snapshot.
	 * Used to determine if auto-rebuild is needed after cage movement.
	 * @param OldScannedAssets Previous scanned assets to compare against
	 * @return True if assets have changed
	 */
	bool HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const;

	/** Extract material overrides from a static mesh component */
	static void ExtractMaterialOverrides(const UStaticMeshComponent* MeshComponent, TArray<FPCGExValencyMaterialOverride>& OutOverrides);

	/** Record a material variant for a mesh asset */
	void RecordMaterialVariant(const FSoftObjectPath& MeshPath, const TArray<FPCGExValencyMaterialOverride>& Overrides);

private:
	/** Transient ghost mesh components for mirror preview */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GhostMeshComponents;
};

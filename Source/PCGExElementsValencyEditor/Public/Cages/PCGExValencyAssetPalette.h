// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCage.h"
#include "GameFramework/Actor.h"
#include "Core/PCGExValencyCommon.h"

#include "PCGExValencyAssetPalette.generated.h"

class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;
class UShapeComponent;
class APCGExValencyCage;

/**
 * Shape types for asset palette detection bounds.
 */
UENUM(BlueprintType)
enum class EPCGExAssetPaletteShape : uint8
{
	Box,
	Sphere,
	Capsule
};

/**
 * A lightweight asset container for Valency.
 *
 * Asset Palettes hold collections of assets that can be mirrored by cages.
 * Unlike cages, they have no orbital connections or volume membership.
 * They exist purely as reusable asset libraries that cages can reference.
 *
 * Use cases:
 * - Define a library of wall pieces that multiple cages can reference
 * - Group variants of an asset type (different materials, sizes)
 * - Organize assets outside the main authoring volume
 */
UCLASS(HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyAssetPalette : public AActor
{
	GENERATED_BODY()

public:
	APCGExValencyAssetPalette();

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin AActor Interface
	virtual void PostActorCreated() override;
	virtual void PostInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	/** Get the display name for this palette */
	FString GetPaletteDisplayName() const;

	/** Get all asset entries (combines manual + scanned) */
	TArray<FPCGExValencyAssetEntry> GetAllAssetEntries() const;

	/** Get manual asset entries only */
	const TArray<FPCGExValencyAssetEntry>& GetManualAssetEntries() const { return ManualAssetEntries; }

	/** Get scanned asset entries only */
	const TArray<FPCGExValencyAssetEntry>& GetScannedAssetEntries() const { return ScannedAssetEntries; }

	/** Check if an actor is inside this palette's detection bounds */
	UFUNCTION(BlueprintNativeEvent, Category = "Palette|Detection")
	bool IsActorInside(AActor* Actor) const;
	virtual bool IsActorInside_Implementation(AActor* Actor) const;

	/** Check if a world location is inside this palette's detection bounds */
	UFUNCTION(BlueprintNativeEvent, Category = "Palette|Detection")
	bool ContainsPoint(const FVector& WorldLocation) const;
	virtual bool ContainsPoint_Implementation(const FVector& WorldLocation) const;

	/** Scan for assets within bounds and register them */
	void ScanAndRegisterContainedAssets();

	/** Compute the local transform to preserve based on flags */
	FTransform ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const;

	/** Get discovered material variants */
	const TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>>& GetDiscoveredMaterialVariants() const
	{
		return DiscoveredMaterialVariants;
	}

	/** Check if specific local transform components should be preserved */
	bool ShouldPreserveTranslation() const;
	bool ShouldPreserveRotation() const;
	bool ShouldPreserveScale() const;

	/** Get the detection shape component */
	UShapeComponent* GetShapeComponent() const { return ShapeComponent; }

	/**
	 * Find all cages in the world that mirror this palette.
	 * @param OutCages Array to populate with mirroring cages
	 */
	void FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const;

	/**
	 * Trigger auto-rebuild for cages that mirror this palette.
	 * Only triggers if Valency mode is active and a mirroring cage's volume has bAutoRebuildOnChange.
	 * @return True if a rebuild was triggered
	 */
	bool TriggerAutoRebuildForMirroringCages();

	/**
	 * Check if scanned assets have changed compared to a previous snapshot.
	 * @param OldScannedAssets Previous scanned assets to compare against
	 * @return True if assets have changed
	 */
	bool HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const;

public:
	/** Human-readable name for this palette */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette")
	FString PaletteName;

	/** Color for editor visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette")
	FLinearColor PaletteColor = FLinearColor(0.8f, 0.5f, 0.2f, 0.5f);

	// ========== Assets ==========

	/**
	 * Manually registered asset entries (user-defined via details panel).
	 * These are persisted and not affected by auto-scanning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Assets", meta = (TitleProperty = "Asset"))
	TArray<FPCGExValencyAssetEntry> ManualAssetEntries;

	/**
	 * Auto-scanned asset entries (transient, rebuilt by ScanAndRegisterContainedAssets).
	 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Palette|Assets")
	TArray<FPCGExValencyAssetEntry> ScannedAssetEntries;

	// ========== Detection ==========

	/** Shape used for containment detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection")
	EPCGExAssetPaletteShape DetectionShape = EPCGExAssetPaletteShape::Box;

	/** Size of the detection bounds (interpretation depends on shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection")
	FVector DetectionExtent = FVector(100.0f);

	/** Whether to automatically scan for and register contained assets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection")
	bool bAutoRegisterContainedAssets = true;

	/**
	 * When enabled, preserves the spatial relationship between assets and the palette center.
	 * Each unique Asset + LocalTransform combination becomes a separate module variant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection")
	bool bPreserveLocalTransforms = false;

	/**
	 * Which components of the local transform to preserve.
	 * Only used when bPreserveLocalTransforms is enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection", meta = (Bitmask, BitmaskEnum = "/Script/PCGExElementsValencyEditor.EPCGExLocalTransformFlags", EditCondition = "bPreserveLocalTransforms"))
	uint8 LocalTransformFlags = static_cast<uint8>(EPCGExLocalTransformFlags::All);

	// ========== Module Settings ==========

	/**
	 * Module settings applied to all assets from this palette.
	 * These settings are used when building rules from cages that mirror this palette.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Module")
	FPCGExValencyModuleSettings ModuleSettings;

protected:
	/** Update the shape component to match DetectionShape and DetectionExtent */
	void UpdateShapeComponent();

	/** Called when asset registration changes */
	virtual void OnAssetRegistrationChanged();

	/** Extract material overrides from a static mesh component */
	static void ExtractMaterialOverrides(const UStaticMeshComponent* MeshComponent, TArray<FPCGExValencyMaterialOverride>& OutOverrides);

	/** Record a material variant for a mesh asset */
	void RecordMaterialVariant(const FSoftObjectPath& MeshPath, const TArray<FPCGExValencyMaterialOverride>& Overrides);

private:
	/** The shape component used for detection bounds visualization */
	UPROPERTY(Transient)
	TObjectPtr<UShapeComponent> ShapeComponent;

	/** Material variants discovered during asset scanning */
	TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>> DiscoveredMaterialVariants;
};

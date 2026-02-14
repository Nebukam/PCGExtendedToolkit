// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyAssetContainerBase.h"
#include "Core/PCGExValencyCommon.h"

#include "PCGExValencyAssetPalette.generated.h"

class UBoxComponent;
class APCGExValencyCage;

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
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyAssetPalette : public APCGExValencyAssetContainerBase
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
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	//~ Begin APCGExValencyEditorActorBase Interface
	virtual void OnRebuildMetaTagTriggered() override;
	virtual void OnPostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End APCGExValencyEditorActorBase Interface

	//~ Begin APCGExValencyAssetContainerBase Interface
	virtual bool IsActorInside_Implementation(AActor* Actor) const override;
	virtual bool ContainsPoint_Implementation(const FVector& WorldLocation) const override;
	virtual TArray<FPCGExValencyAssetEntry> GetAllAssetEntries() const override;
	//~ End APCGExValencyAssetContainerBase Interface

	/** Get the display name for this palette */
	FString GetPaletteDisplayName() const;

	/** Scan for assets within bounds and register them */
	void ScanAndRegisterContainedAssets();

	/** Get the detection box component */
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	/**
	 * Trigger auto-rebuild for cages that mirror this palette.
	 * Only triggers if Valency mode is active and a mirroring cage's volume has bAutoRebuildOnChange.
	 * @return True if a rebuild was triggered
	 * @deprecated Use RequestRebuildForMirroringCages() instead
	 */
	bool TriggerAutoRebuildForMirroringCages();

	/**
	 * Request rebuild for all cages that mirror this palette through the unified dirty state system.
	 * This is the preferred method for triggering rebuilds from palette changes.
	 */
	void RequestRebuildForMirroringCages();

public:
	/** Human-readable name for this palette */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette")
	FString PaletteName;

	/** Color for editor visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette")
	FLinearColor PaletteColor = FLinearColor(0.8f, 0.5f, 0.2f, 0.5f);

	/** Size of the detection box (half-extents) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette|Detection")
	FVector DetectionExtent = FVector(100.0f);

	/**
	 * Ensure the palette's scanned content is up-to-date.
	 * Called lazily when content is accessed after level load.
	 * Safe to call multiple times - only scans if needed.
	 */
	void EnsureInitialized();

	/**
	 * Check if this palette needs initialization (first access after level load).
	 */
	bool NeedsInitialization() const { return bNeedsInitialScan; }

protected:
	/** Update the box component to match DetectionExtent and PaletteColor */
	void UpdateShapeComponent();

	//~ Begin APCGExValencyAssetContainerBase Interface
	virtual void OnAssetRegistrationChanged() override;
	//~ End APCGExValencyAssetContainerBase Interface

private:
	/** The box component used for detection bounds visualization */
	UPROPERTY(VisibleAnywhere, Category = "Palette|Detection")
	TObjectPtr<UBoxComponent> BoxComponent;

	/**
	 * Flag indicating palette needs initial scan after level load.
	 * Transient = true after deserialization, cleared after first scan.
	 */
	UPROPERTY(Transient)
	bool bNeedsInitialScan = true;
};

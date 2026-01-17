// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"

#include "PCGExValencyCage.generated.h"

/**
 * A standard Valency cage that can register assets.
 * Assets placed inside or parented to this cage are registered as valid modules
 * for the cage's orbital configuration.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Valency Cage"))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCage : public APCGExValencyCageBase
{
	GENERATED_BODY()

public:
	APCGExValencyCage();

	//~ Begin APCGExValencyCageBase Interface
	virtual FString GetCageDisplayName() const override;
	virtual bool IsNullCage() const override { return false; }
	//~ End APCGExValencyCageBase Interface

	/** Get all registered assets for this cage */
	const TArray<TSoftObjectPtr<UObject>>& GetRegisteredAssets() const { return RegisteredAssets; }

	/** Register an asset as valid for this cage configuration */
	void RegisterAsset(const TSoftObjectPtr<UObject>& Asset);

	/** Unregister an asset */
	void UnregisterAsset(const TSoftObjectPtr<UObject>& Asset);

	/** Clear all registered assets */
	void ClearRegisteredAssets();

	/** Scan for assets within cage bounds and register them */
	void ScanAndRegisterContainedAssets();

	/** Get the bounds used for asset detection */
	FBox GetAssetDetectionBounds() const;

public:
	/**
	 * Assets registered as valid for this cage's orbital configuration.
	 * Multiple assets = all are valid modules for this config.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Assets")
	TArray<TSoftObjectPtr<UObject>> RegisteredAssets;

	/**
	 * Mirror source cage.
	 * If set, this cage is treated as having the same content as the source.
	 * Useful for reusing configurations without duplicating assets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Assets", AdvancedDisplay)
	TWeakObjectPtr<APCGExValencyCage> MirrorSource;

	/**
	 * Bounds size for asset detection (centered on cage).
	 * Used when scanning for contained assets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (ClampMin = "0.0"))
	FVector AssetDetectionBounds = FVector(100.0f);

	/**
	 * Whether to automatically scan for and register contained assets.
	 * If false, assets must be manually registered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection")
	bool bAutoRegisterContainedAssets = true;

protected:
	/** Called when asset registration changes */
	virtual void OnAssetRegistrationChanged();
};

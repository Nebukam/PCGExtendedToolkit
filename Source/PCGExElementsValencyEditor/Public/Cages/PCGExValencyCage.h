// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"

#include "PCGExValencyCage.generated.h"

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
	 * Whether to automatically scan for and register contained assets.
	 * If false, assets must be manually registered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection")
	bool bAutoRegisterContainedAssets = true;

protected:
	/** Called when asset registration changes */
	virtual void OnAssetRegistrationChanged();
};

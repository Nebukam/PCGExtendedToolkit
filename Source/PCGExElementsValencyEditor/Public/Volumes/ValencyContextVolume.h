// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Core/PCGExValencyOrbitalSet.h"

#include "ValencyContextVolume.generated.h"

/**
 * Volume that defines an authoring context for Valency cages.
 * Cages within this volume inherit their BondingRules and OrbitalSet from the volume.
 *
 * Multiple overlapping volumes are supported - cages in overlap register to all contexts.
 */
UCLASS(HideCategories = (Collision, Brush, Attachment, Physics, Volume))
class PCGEXELEMENTSVALENCYEDITOR_API AValencyContextVolume : public AVolume
{
	GENERATED_BODY()

public:
	AValencyContextVolume();

	//~ Begin AActor Interface
	virtual void PostActorCreated() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	//~ End AActor Interface

	/** Get the bonding rules for this volume */
	UPCGExValencyBondingRules* GetBondingRules() const { return BondingRules; }

	/** Get the orbital set (either override or from BondingRules) */
	UPCGExValencyOrbitalSet* GetEffectiveOrbitalSet() const;

	/** Get default probe radius for cages in this volume */
	float GetDefaultProbeRadius() const { return DefaultProbeRadius; }

	/** Check if a world location is inside this volume */
	bool ContainsPoint(const FVector& WorldLocation, float Tolerance = 0.0f) const;

	/** Collect all cages currently inside this volume */
	void CollectContainedCages(TArray<class APCGExValencyCageBase*>& OutCages) const;

	/** Trigger a rebuild of the BondingRules from contained cages */
	void RebuildBondingRules();

	/**
	 * Build bonding rules from all cages in this volume.
	 * Updates the BondingRules asset in place.
	 */
	UFUNCTION(CallInEditor, Category = "Valency")
	void BuildRulesFromCages();

	/**
	 * Force refresh all cage relationships within this volume.
	 * Useful when cages have been moved or modified externally.
	 */
	UFUNCTION(CallInEditor, Category = "Valency")
	void RefreshCageRelationships();

public:
	/** The bonding rules data asset for this context. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency")
	TObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/**
	 * Optional orbital set override.
	 * If not set, uses the first orbital set from BondingRules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency", AdvancedDisplay)
	TObjectPtr<UPCGExValencyOrbitalSet> OrbitalSetOverride;

	/**
	 * Default probe radius for cages in this volume.
	 * Cages use this value unless they have their own override.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Detection", meta = (ClampMin = "0.0"))
	float DefaultProbeRadius = 100.0f;

	/** Debug color for visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Debug")
	FLinearColor DebugColor = FLinearColor(0.3f, 0.3f, 0.8f, 0.5f);

	/** Whether to automatically rebuild rules when cages change */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Building")
	bool bAutoRebuildOnChange = false;

	/**
	 * Actors with PCG components to regenerate after building rules.
	 * When BuildRulesFromCages completes, PCG components on these actors will be flushed and regenerated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Building")
	TArray<TObjectPtr<AActor>> PCGActorsToRegenerate;

	/**
	 * When enabled and in Valency Mode, automatically tracks selected actors.
	 * When actors are moved into/out of cages, the cage's scanned assets are updated.
	 * Only active when the Valency editor mode is enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Detection")
	bool bAutoTrackAssetPlacement = false;

	/**
	 * Actor tags to ignore during scanning and detection.
	 * Actors with any of these tags will be completely excluded from the building process.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Filtering")
	TArray<FName> IgnoredActorTags = {FName("PCGEx_Ignore")};

	/**
	 * Specific actors to ignore during scanning and detection.
	 * These actors (and their attached children) will be completely excluded.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Filtering")
	TArray<TObjectPtr<AActor>> IgnoredActors;

	/**
	 * When enabled, automatically ignores actors spawned by PCGActorsToRegenerate.
	 * Any actor attached to (child of) a PCG actor in the regenerate list will be excluded from scanning.
	 * Default: true
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Filtering")
	bool bAutoIgnorePCGSpawnedActors = true;

	/**
	 * Check if an actor should be ignored based on the ignore rules.
	 * @param Actor The actor to check
	 * @return True if the actor should be ignored/excluded
	 */
	bool ShouldIgnoreActor(const AActor* Actor) const;

	/**
	 * Find all volumes in the current level that reference the same BondingRules.
	 * @param OutVolumes Array that will be filled with related volumes (includes this volume)
	 */
	void FindRelatedVolumes(TArray<AValencyContextVolume*>& OutVolumes) const;

	/**
	 * Check if the Valency editor mode is currently active.
	 * Auto-rebuild features only trigger when this returns true.
	 * @return True if the Valency editor mode is active
	 */
	static bool IsValencyModeActive();

protected:
	/** Regenerate PCG components on actors in PCGActorsToRegenerate list */
	void RegeneratePCGActors();
	/** Notify contained cages that volume properties changed */
	void NotifyContainedCages();
};

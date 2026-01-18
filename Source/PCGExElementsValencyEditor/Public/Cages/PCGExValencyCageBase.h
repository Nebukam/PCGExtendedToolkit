// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGExValencyCageOrbital.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Core/PCGExValencyBondingRules.h"

#include "PCGExValencyCageBase.generated.h"

class AValencyContextVolume;

/**
 * Abstract base class for Valency cage actors.
 * Cages represent potential node positions in a Valency graph and define
 * orbital connections to neighboring cages.
 *
 * Cages inherit their BondingRules and OrbitalSet from containing volumes
 * unless an explicit override is provided.
 */
UCLASS(Abstract, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCageBase : public AActor
{
	GENERATED_BODY()

public:
	APCGExValencyCageBase();

	//~ Begin AActor Interface
	virtual void PostActorCreated() override;
	virtual void PostInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	//~ Begin Cage Interface

	/** Get the display name for this cage (used in editor UI) */
	virtual FString GetCageDisplayName() const;

	/** Whether this is a null cage (boundary marker) */
	virtual bool IsNullCage() const { return false; }

	/** Get the effective orbital set (from volume or override) */
	UPCGExValencyOrbitalSet* GetEffectiveOrbitalSet() const;

	/** Get the effective bonding rules (from volume or override) */
	UPCGExValencyBondingRules* GetEffectiveBondingRules() const;

	/** Get the effective probe radius */
	float GetEffectiveProbeRadius() const;

	/**
	 * Get whether orbital directions should be transformed by this cage's rotation.
	 * Resolves TransformMode (Inherit uses OrbitalSet setting, Force overrides).
	 */
	bool ShouldTransformOrbitalDirections() const;

	/** Get the orbitals array */
	const TArray<FPCGExValencyCageOrbital>& GetOrbitals() const { return Orbitals; }

	/** Get mutable orbitals array */
	TArray<FPCGExValencyCageOrbital>& GetOrbitals() { return Orbitals; }

	/** Check if this cage has a connection to another cage */
	bool HasConnectionTo(const APCGExValencyCageBase* OtherCage) const;

	/** Get the orbital index of a connection to another cage (-1 if not connected) */
	int32 GetOrbitalIndexTo(const APCGExValencyCageBase* OtherCage) const;

	/** Called when a containing volume changes */
	virtual void OnContainingVolumeChanged(AValencyContextVolume* Volume);

	//~ End Cage Interface

	/** Recalculate which volumes contain this cage */
	void RefreshContainingVolumes();

	/** Check if an actor should be ignored based on containing volumes' ignore rules */
	bool ShouldIgnoreActor(const AActor* Actor) const;

	/** Initialize orbitals from the orbital set */
	void InitializeOrbitalsFromSet();

	/** Detect and connect to nearby cages using probe radius */
	void DetectNearbyConnections();

	/**
	 * Remove null/invalid entries from all orbital manual connection lists.
	 * @return Total number of stale entries removed
	 */
	int32 CleanupManualConnections();

	/**
	 * Notify this cage that a related cage has moved or changed.
	 * Triggers a refresh of connections if the moved cage affects us.
	 * @param MovedCage The cage that was moved/changed
	 */
	void OnRelatedCageMoved(APCGExValencyCageBase* MovedCage);

	/**
	 * Notify all cages in the world that this cage has moved.
	 * Called automatically from PostEditMove.
	 * @deprecated Use NotifyAffectedCagesOfMovement for better performance
	 */
	void NotifyAllCagesOfMovement();

	/**
	 * Notify only cages affected by this cage's movement using spatial registry.
	 * More efficient than NotifyAllCagesOfMovement for large scenes.
	 * @param OldPosition Position before the move
	 * @param NewPosition Position after the move
	 */
	void NotifyAffectedCagesOfMovement(const FVector& OldPosition, const FVector& NewPosition);

	/**
	 * Set visibility of internal debug components.
	 * Called by editor mode to hide built-in visuals when custom mode drawing is active.
	 * @param bVisible True to show components, false to hide
	 */
	virtual void SetDebugComponentsVisible(bool bVisible);

public:
	/** Optional display name for this cage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage")
	FString CageName;

	/**
	 * Optional explicit BondingRules override.
	 * If not set, uses the BondingRules from containing volume(s).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Advanced", AdvancedDisplay)
	TObjectPtr<UPCGExValencyBondingRules> BondingRulesOverride;

	/**
	 * Optional explicit OrbitalSet override.
	 * If not set, uses the OrbitalSet from containing volume(s).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Advanced", AdvancedDisplay)
	TObjectPtr<UPCGExValencyOrbitalSet> OrbitalSetOverride;

	/**
	 * Probe radius for detecting nearby cages.
	 * -1 = use volume's default radius.
	 * 0 = receive-only (other cages can detect me, I don't detect them).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (ClampMin = "-1.0"))
	float ProbeRadius = -1.0f;

	/**
	 * Whether to apply cage rotation to orbital directions.
	 * If true, orbital directions are transformed by this cage's rotation.
	 * If false, orbitals use world-space directions (useful for copy-paste patterns).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Transform")
	bool bTransformOrbitalDirections = true;

	/** Orbital connections to other cages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Orbitals", meta = (TitleProperty = "OrbitalName"))
	TArray<FPCGExValencyCageOrbital> Orbitals;

protected:
	/** Volumes that contain this cage (transient, not saved) */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Cage|Debug")
	TArray<TWeakObjectPtr<AValencyContextVolume>> ContainingVolumes;

	/** Cached orbital set (resolved from volumes or override) */
	UPROPERTY(Transient)
	TWeakObjectPtr<UPCGExValencyOrbitalSet> CachedOrbitalSet;

	/** Whether orbital initialization is needed */
	bool bNeedsOrbitalInit = true;

	/** Last position used for live drag updates (throttling) */
	FVector LastDragUpdatePosition = FVector::ZeroVector;

	/** Minimum distance to trigger a live drag update */
	static constexpr float DragUpdateThreshold = 10.0f;

	/** Whether we're currently being dragged */
	bool bIsDragging = false;

	/** Position when drag started (for computing affected cages) */
	FVector DragStartPosition = FVector::ZeroVector;

	/** Update connections during drag using spatial registry */
	void UpdateConnectionsDuringDrag();
};

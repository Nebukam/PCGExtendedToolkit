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
UCLASS(Abstract, NotPlaceable, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCageBase : public AActor
{
	GENERATED_BODY()

public:
	APCGExValencyCageBase();

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
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

	/** Initialize orbitals from the orbital set */
	void InitializeOrbitalsFromSet();

	/** Detect and connect to nearby cages using probe radius */
	void DetectNearbyConnections();

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
};

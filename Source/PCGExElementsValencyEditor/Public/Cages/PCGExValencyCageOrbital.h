// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValencyCageOrbital.generated.h"

class APCGExValencyCageBase;

/**
 * Represents an orbital connection on a cage.
 * Each orbital can connect to another cage via manual or auto-detected connections.
 * Manual connections take priority and exclude the target from auto-detection.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageOrbital
{
	GENERATED_BODY()

	/** Index of this orbital in the OrbitalSet (0-63) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbital")
	int32 OrbitalIndex = -1;

	/** Name of this orbital (from OrbitalSet, for display) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbital")
	FName OrbitalName;

	/**
	 * Manually assigned valid connections for this orbital (serialized).
	 * All cages in this list are excluded from auto-detection.
	 * Use this to define explicit connections regardless of spatial proximity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital|Manual")
	TArray<TObjectPtr<APCGExValencyCageBase>> ManualConnections;

	/** Auto-detected cage connection (transient - rebuilt on level load/move) */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Orbital|Auto")
	TWeakObjectPtr<APCGExValencyCageBase> AutoConnectedCage;

	/** Whether this orbital connection is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital")
	bool bEnabled = true;

	FPCGExValencyCageOrbital() = default;

	FPCGExValencyCageOrbital(int32 InIndex, const FName& InName)
		: OrbitalIndex(InIndex)
		, OrbitalName(InName)
	{
	}

	/** Check if this orbital has any valid connection (manual or auto) */
	bool HasConnection() const { return bEnabled && (HasManualConnections() || AutoConnectedCage.IsValid()); }

	/** Check if this orbital has any manual connections */
	bool HasManualConnections() const { return ManualConnections.Num() > 0; }

	/** Get the first manual connection (for compatibility/display) */
	APCGExValencyCageBase* GetFirstManualConnection() const
	{
		for (const TObjectPtr<APCGExValencyCageBase>& Cage : ManualConnections)
		{
			if (Cage) return Cage;
		}
		return nullptr;
	}

	/** Get the auto-detected connection */
	APCGExValencyCageBase* GetAutoConnection() const { return AutoConnectedCage.Get(); }

	/** Get display connection (first manual if any, else auto) - for visualization */
	APCGExValencyCageBase* GetDisplayConnection() const
	{
		if (APCGExValencyCageBase* Manual = GetFirstManualConnection())
		{
			return Manual;
		}
		return AutoConnectedCage.Get();
	}

	/** Check if a specific cage is in the manual list (used to exclude from auto-detection) */
	bool IsManualTarget(const APCGExValencyCageBase* Cage) const
	{
		if (!Cage) return false;
		for (const TObjectPtr<APCGExValencyCageBase>& ManualCage : ManualConnections)
		{
			if (ManualCage == Cage) return true;
		}
		return false;
	}

	/** Remove null/invalid entries from manual connections. Returns number removed. */
	int32 CleanupManualConnections()
	{
		return ManualConnections.RemoveAll([](const TObjectPtr<APCGExValencyCageBase>& Cage)
		{
			return Cage == nullptr;
		});
	}

	// Legacy accessor - returns display connection
	APCGExValencyCageBase* Get() const { return GetDisplayConnection(); }
};

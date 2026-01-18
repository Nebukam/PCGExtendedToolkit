// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValencyCageOrbital.generated.h"

class APCGExValencyCageBase;

/**
 * Represents an orbital connection on a cage.
 * Each orbital can connect to another cage.
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

	/** The cage connected via this orbital (serialized with the level) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital")
	TObjectPtr<APCGExValencyCageBase> ConnectedCage;

	/** Whether this orbital connection is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital")
	bool bEnabled = true;

	FPCGExValencyCageOrbital() = default;

	FPCGExValencyCageOrbital(int32 InIndex, const FName& InName)
		: OrbitalIndex(InIndex)
		, OrbitalName(InName)
	{
	}

	/** Check if this orbital has a valid connection */
	bool HasConnection() const { return bEnabled && ConnectedCage != nullptr; }

	/** Get the connected cage (may be null) */
	APCGExValencyCageBase* Get() const { return ConnectedCage; }
};

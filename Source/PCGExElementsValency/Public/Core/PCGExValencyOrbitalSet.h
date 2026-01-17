// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"

#include "PCGExValencyOrbitalSet.generated.h"

/**
 * A single orbital entry in a Valency orbital set.
 * References a BitmaskCollection entry for direction and bitmask data.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyOrbitalEntry
{
	GENERATED_BODY()

	/** Reference to bitmask collection entry - provides direction and bitmask */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FPCGExBitmaskRef BitmaskRef;

	/** Display name for UI (defaults to BitmaskRef.Identifier if empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FText DisplayName;

	/** Debug visualization color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FLinearColor DebugColor = FLinearColor::White;

	/** Get the orbital name (from BitmaskRef identifier) */
	FName GetOrbitalName() const { return BitmaskRef.Identifier; }

	/** Get the display name, falling back to identifier if empty */
	FText GetDisplayName() const
	{
		return DisplayName.IsEmpty() ? FText::FromName(BitmaskRef.Identifier) : DisplayName;
	}

	/** Get direction and bitmask from the referenced collection entry */
	bool GetDirectionAndBitmask(FVector& OutDirection, int64& OutBitmask) const
	{
		FPCGExSimpleBitmask SimpleBitmask;
		if (BitmaskRef.TryGetAdjacencyInfos(OutDirection, SimpleBitmask))
		{
			OutBitmask = SimpleBitmask.Bitmask;
			return true;
		}
		return false;
	}
};

class UPCGExValencyOrbitalSet;

/**
 * Sentinel value for orbital indices indicating no match.
 */
namespace PCGExValency
{
	constexpr uint8 NO_ORBITAL_MATCH = 0xFF;

	/**
	 * Cached orbital data for fast lookup during processing.
	 * Pre-resolves all BitmaskRef lookups once to avoid repeated asset access.
	 */
	struct PCGEXELEMENTSVALENCY_API FOrbitalCache
	{
		/** Pre-resolved directions (normalized) */
		TArray<FVector> Directions;

		/** Pre-resolved bitmasks */
		TArray<int64> Bitmasks;

		/** Dot threshold for matching (pre-computed from angle) */
		double DotThreshold = 0.0;

		/** Whether to transform directions using point transform */
		bool bTransformOrbital = true;

		/** Number of cached orbitals */
		int32 Num() const { return Directions.Num(); }

		/** Check if cache is valid (has been built) */
		bool IsValid() const { return Directions.Num() > 0; }

		/**
		 * Build cache from orbital set.
		 * @param OrbitalSet The orbital set to cache
		 * @return True if all orbitals resolved successfully
		 */
		bool BuildFrom(const UPCGExValencyOrbitalSet* OrbitalSet);

		/**
		 * Find matching orbital index using cached data.
		 * @param InDirection The direction to match (normalized)
		 * @param bUseTransform If true, directions are compared in local space
		 * @param InTransform The transform to use if bUseTransform is true
		 * @return Orbital index (0-254), or NO_ORBITAL_MATCH (0xFF) if no match
		 */
		uint8 FindMatchingOrbital(const FVector& InDirection, bool bUseTransform, const FTransform& InTransform) const;

		/** Get the bitmask for an orbital index (must be valid index) */
		int64 GetBitmask(int32 OrbitalIndex) const { return Bitmasks[OrbitalIndex]; }

		/** Get the direction for an orbital index (must be valid index) */
		const FVector& GetDirection(int32 OrbitalIndex) const { return Directions[OrbitalIndex]; }
	};
}

/**
 * A collection of orbital definitions for a Valency layer.
 * Used by "Write Valency Orbitals" node to compute orbital masks and indices.
 */
UCLASS(BlueprintType, DisplayName="[PCGEx] Valency Orbital Set")
class PCGEXELEMENTSVALENCY_API UPCGExValencyOrbitalSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Layer name - determines attribute naming (PCGEx/Valency/Mask/{LayerName}, PCGEx/Valency/Idx/{LayerName}) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName LayerName = FName("Main");

	/** Angle threshold in degrees for direction matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "90.0"))
	double AngleThreshold = 22.5;

	/** Whether to transform directions using the vertex point transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bTransformDirection = true;

	/** Orbital definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (TitleProperty = "{BitmaskRef}"))
	TArray<FPCGExValencyOrbitalEntry> Orbitals;

	/** Get the number of orbitals */
	int32 Num() const { return Orbitals.Num(); }

	/** Check if an orbital index is valid */
	bool IsValidIndex(int32 Index) const { return Orbitals.IsValidIndex(Index); }

	/** Get the attribute name for vertex orbital mask */
	FName GetOrbitalMaskAttributeName() const
	{
		return FName(FString::Printf(TEXT("PCGEx/Valency/Mask/%s"), *LayerName.ToString()));
	}

	/** Get the attribute name for edge orbital indices */
	FName GetOrbitalIdxAttributeName() const
	{
		return FName(FString::Printf(TEXT("PCGEx/Valency/Idx/%s"), *LayerName.ToString()));
	}

	/**
	 * Validate the orbital set - checks for duplicate bitmasks and valid BitmaskRefs.
	 * @param OutErrors Array to receive error messages
	 * @return True if validation passed
	 */
	bool Validate(TArray<FText>& OutErrors) const;

	/**
	 * Find the orbital index that matches a given direction.
	 * @param InDirection The direction to match (normalized)
	 * @param bUseTransform If true, directions are compared in local space
	 * @param InTransform The transform to use if bUseTransform is true
	 * @return Orbital index (0-254), or NO_ORBITAL_MATCH (0xFF) if no match
	 */
	uint8 FindMatchingOrbital(const FVector& InDirection, bool bUseTransform = false, const FTransform& InTransform = FTransform::Identity) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

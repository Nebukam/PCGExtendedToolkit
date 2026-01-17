// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"

#include "PCGExValenceSocketCollection.generated.h"

/**
 * A single socket entry in a Valence socket collection.
 * References a BitmaskCollection entry for direction and bitmask data.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCE_API FPCGExValenceSocketEntry
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

	/** Get the socket name (from BitmaskRef identifier) */
	FName GetSocketName() const { return BitmaskRef.Identifier; }

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

class UPCGExValenceSocketCollection;

/**
 * Sentinel value for socket indices indicating no match.
 */
namespace PCGExValence
{
	constexpr uint8 NO_SOCKET_MATCH = 0xFF;

	/**
	 * Cached socket data for fast lookup during processing.
	 * Pre-resolves all BitmaskRef lookups once to avoid repeated asset access.
	 */
	struct PCGEXELEMENTSVALENCE_API FSocketCache
	{
		/** Pre-resolved directions (normalized) */
		TArray<FVector> Directions;

		/** Pre-resolved bitmasks */
		TArray<int64> Bitmasks;

		/** Dot threshold for matching (pre-computed from angle) */
		double DotThreshold = 0.0;

		/** Whether to transform directions using point transform */
		bool bTransformDirection = true;

		/** Number of cached sockets */
		int32 Num() const { return Directions.Num(); }

		/** Check if cache is valid (has been built) */
		bool IsValid() const { return Directions.Num() > 0; }

		/**
		 * Build cache from socket collection.
		 * @param Collection The socket collection to cache
		 * @return True if all sockets resolved successfully
		 */
		bool BuildFrom(const UPCGExValenceSocketCollection* Collection);

		/**
		 * Find matching socket index using cached data.
		 * @param InDirection The direction to match (normalized)
		 * @param bUseTransform If true, directions are compared in local space
		 * @param InTransform The transform to use if bUseTransform is true
		 * @return Socket index (0-254), or NO_SOCKET_MATCH (0xFF) if no match
		 */
		uint8 FindMatchingSocket(const FVector& InDirection, bool bUseTransform, const FTransform& InTransform) const;

		/** Get the bitmask for a socket index (must be valid index) */
		int64 GetBitmask(int32 SocketIndex) const { return Bitmasks[SocketIndex]; }

		/** Get the direction for a socket index (must be valid index) */
		const FVector& GetDirection(int32 SocketIndex) const { return Directions[SocketIndex]; }
	};
}

/**
 * A collection of socket definitions for a Valence layer.
 * Used by "Write Valence Sockets" node to compute socket masks and indices.
 */
UCLASS(BlueprintType, DisplayName="[PCGEx] Valence Socket Collection")
class PCGEXELEMENTSVALENCE_API UPCGExValenceSocketCollection : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Layer name - determines attribute naming (PCGEx/Valence/Mask/{LayerName}, PCGEx/Valence/Idx/{LayerName}) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName LayerName = FName("Main");

	/** Angle threshold in degrees for direction matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "90.0"))
	double AngleThreshold = 22.5;

	/** Whether to transform directions using the vertex point transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bTransformDirection = true;

	/** Socket definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (TitleProperty = "{BitmaskRef}"))
	TArray<FPCGExValenceSocketEntry> Sockets;

	/** Get the number of sockets */
	int32 Num() const { return Sockets.Num(); }

	/** Check if a socket index is valid */
	bool IsValidIndex(int32 Index) const { return Sockets.IsValidIndex(Index); }

	/** Get the attribute name for vertex socket mask */
	FName GetMaskAttributeName() const
	{
		return FName(FString::Printf(TEXT("PCGEx/Valence/Mask/%s"), *LayerName.ToString()));
	}

	/** Get the attribute name for edge socket indices */
	FName GetIdxAttributeName() const
	{
		return FName(FString::Printf(TEXT("PCGEx/Valence/Idx/%s"), *LayerName.ToString()));
	}

	/**
	 * Validate the collection - checks for duplicate bitmasks and valid BitmaskRefs.
	 * @param OutErrors Array to receive error messages
	 * @return True if validation passed
	 */
	bool Validate(TArray<FText>& OutErrors) const;

	/**
	 * Find the socket index that matches a given direction.
	 * @param InDirection The direction to match (normalized)
	 * @param bUseTransform If true, directions are compared in local space
	 * @param InTransform The transform to use if bUseTransform is true
	 * @return Socket index (0-254), or NO_SOCKET_MATCH (0xFF) if no match
	 */
	uint8 FindMatchingSocket(const FVector& InDirection, bool bUseTransform = false, const FTransform& InTransform = FTransform::Identity) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

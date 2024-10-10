// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMath.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Seed Components"))
enum class EPCGExSeedComponents : uint8
{
	None      = 0,
	Local     = 1 << 1 UMETA(DisplayName = "Local"),
	Settings  = 1 << 2 UMETA(DisplayName = "Settings"),
	Component = 1 << 3 UMETA(DisplayName = "Component"),
};

ENUM_CLASS_FLAGS(EPCGExSeedComponents)
using EPCGExSeedComponentsBitmask = TEnumAsByte<EPCGExSeedComponents>;

namespace PCGExRandom
{
	FORCEINLINE static int ComputeSeed(const int A)
	{
		// From Epic git main, unexposed in 5.3
		return (A * 196314165U) + 907633515U;
	}

	FORCEINLINE static int ComputeSeed(const int A, const int B)
	{
		// From Epic git main, unexposed in 5.3
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U);
	}

	FORCEINLINE static int ComputeSeed(const int A, const int B, const int C)
	{
		// From Epic git main, unexposed in 5.3
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U) ^ ((C * 34731343U) + 453816743U);
	}

	FORCEINLINE static int32 GetSeedFromPoint(const uint8 Flags, const FPCGPoint& Point, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr)
	{
		int32 Seed = Point.Seed;

		const bool bHasLocalFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Local)) != 0;
		const bool bHasSettingsFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Settings)) != 0;
		const bool bHasComponentFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Component)) != 0;

		if (bHasLocalFlag)
		{
			Seed = ComputeSeed(Seed, Local);
		}

		if (bHasSettingsFlag || bHasComponentFlag)
		{
			if (Settings && Component)
			{
				Seed = ComputeSeed(Seed, Settings->Seed, Component->Seed);
			}
			else if (Settings)
			{
				Seed = ComputeSeed(Seed, Settings->Seed);
			}
			else if (Component)
			{
				Seed = ComputeSeed(Seed, Component->Seed);
			}
		}

		return Seed;
	}

	FORCEINLINE static int32 GetSeedFromPoint(const FPCGPoint& Point, const int32 Local, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr)
	{
		// From Epic git main, unexposed in 5.3

		int Seed = Point.Seed + Local;

		if (Settings && Component) { Seed = ComputeSeed(Seed, Settings->Seed, Component->Seed); }
		else if (Settings) { Seed = ComputeSeed(Seed, Settings->Seed); }
		else if (Component) { Seed = ComputeSeed(Seed, Component->Seed); }

		return Seed;
	}

	FORCEINLINE static FRandomStream GetRandomStreamFromPoint(const FPCGPoint& Point, const int32 Offset, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr)
	{
		return FRandomStream(GetSeedFromPoint(Point, Offset, Settings, Component));
	}

	FORCEINLINE static int ComputeSeed(const FPCGPoint& Point, const FVector& Offset = FVector::ZeroVector)
	{
		return static_cast<int>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Point.Transform.GetLocation() * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	}
}

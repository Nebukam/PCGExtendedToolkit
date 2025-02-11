// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRandom.h"
#include "PCGExMath.h"

namespace PCGExRandom
{
	int32 GetSeedFromPoint(const uint8 Flags, const FPCGPoint& Point, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
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

	int32 GetSeedFromPoint(const FPCGPoint& Point, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		// From Epic git main, unexposed in 5.3

		int Seed = Point.Seed + Local;

		if (Settings && Component) { Seed = ComputeSeed(Seed, Settings->Seed, Component->Seed); }
		else if (Settings) { Seed = ComputeSeed(Seed, Settings->Seed); }
		else if (Component) { Seed = ComputeSeed(Seed, Component->Seed); }

		return Seed;
	}

	FRandomStream GetRandomStreamFromPoint(const FPCGPoint& Point, const int32 Offset, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		return FRandomStream(GetSeedFromPoint(Point, Offset, Settings, Component));
	}

	int ComputeSeed(const FPCGPoint& Point, const FVector& Offset)
	{
		return static_cast<int>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Point.Transform.GetLocation() * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, MIN_int32, MAX_int32));
	}
}

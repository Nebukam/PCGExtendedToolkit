// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRandom.h"

#include "PCGComponent.h"
#include "PCGExMath.h"
#include "PCGSettings.h"
#include "Data/PCGExPointIO.h"

namespace PCGExRandom
{
	int32 GetSeed(const int32 BaseSeed, const uint8 Flags, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		int32 Seed = BaseSeed;

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

	int32 GetSeed(const int32 BaseSeed, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		// From Epic git main, unexposed in 5.3

		int Seed = BaseSeed + Local;

		if (Settings && Component) { Seed = ComputeSeed(Seed, Settings->Seed, Component->Seed); }
		else if (Settings) { Seed = ComputeSeed(Seed, Settings->Seed); }
		else if (Component) { Seed = ComputeSeed(Seed, Component->Seed); }

		return Seed;
	}

	FRandomStream GetRandomStreamFromPoint(const int32 BaseSeed, const int32 Offset, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		return FRandomStream(GetSeed(BaseSeed, Offset, Settings, Component));
	}

	int ComputeSpatialSeed(const FVector& Origin, const FVector& Offset)
	{
		return static_cast<int>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Origin * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, MIN_int32, MAX_int32));
	}
}

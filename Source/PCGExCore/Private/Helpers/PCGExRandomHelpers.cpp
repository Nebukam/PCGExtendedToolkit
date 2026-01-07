// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExRandomHelpers.h"

#include "PCGComponent.h"
#include "PCGSettings.h"
#include "Helpers/PCGHelpers.h"

namespace PCGExRandomHelpers
{
	int32 GetSeed(const int32 BaseSeed, const uint8 Flags, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		int32 Seed = BaseSeed;

		const bool bHasLocalFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Local)) != 0;
		const bool bHasSettingsFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Settings)) != 0;
		const bool bHasComponentFlag = (Flags & static_cast<uint8>(EPCGExSeedComponents::Component)) != 0;

		if (bHasLocalFlag)
		{
			Seed = PCGHelpers::ComputeSeed(Seed, Local);
		}

		if (bHasSettingsFlag || bHasComponentFlag)
		{
			if (Settings && Component)
			{
				Seed = PCGHelpers::ComputeSeed(Seed, Settings->Seed, Component->Seed);
			}
			else if (Settings)
			{
				Seed = PCGHelpers::ComputeSeed(Seed, Settings->Seed);
			}
			else if (Component)
			{
				Seed = PCGHelpers::ComputeSeed(Seed, Component->Seed);
			}
		}

		return Seed;
	}

	int32 GetSeed(const int32 BaseSeed, const int32 Local, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		// From Epic git main, unexposed in 5.3

		int Seed = BaseSeed + Local;

		if (Settings && Component) { Seed = PCGHelpers::ComputeSeed(Seed, Settings->Seed, Component->Seed); }
		else if (Settings) { Seed = PCGHelpers::ComputeSeed(Seed, Settings->Seed); }
		else if (Component) { Seed = PCGHelpers::ComputeSeed(Seed, Component->Seed); }

		return Seed;
	}

	FRandomStream GetRandomStreamFromPoint(const int32 BaseSeed, const int32 Offset, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		return FRandomStream(GetSeed(BaseSeed, Offset, Settings, Component));
	}

	int ComputeSpatialSeed(const FVector& Origin, const FVector& Offset)
	{
		return PCGHelpers::ComputeSeed(PCGHelpers::ComputeSeedFromPosition(Origin), PCGHelpers::ComputeSeedFromPosition(Offset));
	}
}

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExRandomHelpers.h"

#include "PCGComponent.h"
#include "PCGSettings.h"
#include "Algo/Sort.h"
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
		int32 Seed = PCGHelpers::ComputeSeed(BaseSeed, Local);

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

		return Seed;
	}

	void FSeedResolver::Init(const uint8 Flags, const int32 InLocal, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		bLocal = (Flags & static_cast<uint8>(EPCGExSeedComponents::Local)) != 0;
		Local = InLocal;

		Stage = EStage::None;
		SettingsSeed = 0;
		ComponentSeed = 0;

		const uint8 StageFlags = static_cast<uint8>(EPCGExSeedComponents::Settings) | static_cast<uint8>(EPCGExSeedComponents::Component);
		if ((Flags & StageFlags) == 0)
		{
			return;
		}

		if (Settings)
		{
			SettingsSeed = Settings->Seed;
		}
		if (Component)
		{
			ComponentSeed = Component->Seed;
		}

		if (Settings && Component)
		{
			Stage = EStage::Both;
		}
		else if (Settings)
		{
			Stage = EStage::Settings;
		}
		else if (Component)
		{
			Stage = EStage::Component;
		}
	}

	int32 FSeedResolver::Resolve(const int32 BaseSeed) const
	{
		const int32 Seed = bLocal ? PCGHelpers::ComputeSeed(BaseSeed, Local) : BaseSeed;

		switch (Stage)
		{
		case EStage::Both:
			return PCGHelpers::ComputeSeed(Seed, SettingsSeed, ComponentSeed);
		case EStage::Settings:
			return PCGHelpers::ComputeSeed(Seed, SettingsSeed);
		case EStage::Component:
			return PCGHelpers::ComputeSeed(Seed, ComponentSeed);
		default:
			return Seed;
		}
	}

	FRandomStream GetRandomStreamFromPoint(const int32 BaseSeed, const int32 Offset, const UPCGSettings* Settings, const UPCGComponent* Component)
	{
		return FRandomStream(GetSeed(BaseSeed, Offset, Settings, Component));
	}

	int ComputeSpatialSeed(const FVector& Origin, const FVector& Offset)
	{
		return PCGHelpers::ComputeSeed(PCGHelpers::ComputeSeedFromPosition(Origin), PCGHelpers::ComputeSeedFromPosition(Offset));
	}

	void WeightedPicksWithoutReplacement(FRandomStream& Random, TConstArrayView<double> Weights, const int32 NumPicks, TArray<int32>& OutPicks)
	{
		const int32 NumItems = Weights.Num();
		const int32 Count = FMath::Clamp(NumPicks, 0, NumItems);

		OutPicks.Reset(Count);
		if (Count == 0)
		{
			return;
		}

		struct FKeyedIndex
		{
			double Key;
			int32 Index;
			bool bPositive;
		};

		TArray<FKeyedIndex> Keyed;
		Keyed.SetNumUninitialized(NumItems);

		for (int32 i = 0; i < NumItems; ++i)
		{
			// GetFraction is in [0,1), so 1-u is in (0,1] and the exponential draw stays finite.
			const double Exp = -FMath::Loge(1.0 - static_cast<double>(Random.GetFraction()));
			const double W = Weights[i];
			const bool bPositive = W > 0.0;
			Keyed[i] = FKeyedIndex{bPositive ? Exp / W : Exp, i, bPositive};
		}

		Algo::Sort(Keyed, [](const FKeyedIndex& A, const FKeyedIndex& B)
		{
			if (A.bPositive != B.bPositive)
			{
				return A.bPositive;
			}
			return A.Key < B.Key;
		});

		for (int32 i = 0; i < Count; ++i)
		{
			OutPicks.Add(Keyed[i].Index);
		}
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExDetailsNoise.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "Details/PCGExDetailsSettings.h"
#include "Helpers/PCGHelpers.h"

PCGEX_SETTING_DATA_VALUE_IMPL(FPCGExRandomRatioDetails, Seed, int32, SeedInput, LocalSeed, SeedValue)
PCGEX_SETTING_DATA_VALUE_IMPL(FPCGExRandomRatioDetails, Amount, double, AmountInput, LocalAmount, Amount)
PCGEX_SETTING_DATA_VALUE_IMPL(FPCGExRandomRatioDetails, FixedAmount, int32, AmountInput, LocalAmount, FixedAmount)

int32 FPCGExRandomRatioDetails::GetNumPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems) const
{
	int32 NumPicks = 0;
	if (Units == EPCGExMeanMeasure::Relative)
	{
		NumPicks = FMath::Clamp(FMath::RoundToInt(NumMaxItems * GetValueSettingAmount(InContext, InData)->Read(0)), 0, NumMaxItems);
	}
	else
	{
		NumPicks = FMath::Clamp(GetValueSettingFixedAmount(InContext, InData)->Read(0), 0, NumMaxItems);
	}
	return NumPicks;
}

void FPCGExRandomRatioDetails::GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TSet<int32>& OutPicks) const
{
	TArray<int32> Picks;
	GetPicks(InContext, InData, NumMaxItems, Picks);
	OutPicks.Append(Picks);
}

void FPCGExRandomRatioDetails::GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TArray<int32>& OutPicks) const
{
	const int32 NumPicks = GetNumPicks(InContext, InData, NumMaxItems);
	
	PCGEx::ArrayOfIndices(OutPicks, NumMaxItems);

	FRandomStream Random = PCGHelpers::GetRandomStreamFromSeed(
		PCGHelpers::ComputeSeed(GetValueSettingSeed(InContext, InData)->Read(0)),
		InContext->GetInputSettings<UPCGSettings>(), InContext->ExecutionSource.Get());

	for (int32 i = NumMaxItems - 1; i > 0; --i) { OutPicks.Swap(i, Random.RandRange(0, i)); }
	OutPicks.SetNum(NumPicks);
}

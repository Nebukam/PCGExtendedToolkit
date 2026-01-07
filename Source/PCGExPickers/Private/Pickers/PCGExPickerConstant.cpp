// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Pickers/PCGExPickerConstant.h"

#include "Containers/PCGExManagedObjects.h"
#include "Math/PCGExMath.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePickerConstant"
#define PCGEX_NAMESPACE CreatePickerConstant

PCGEX_PICKER_BOILERPLATE(Constant, {}, {})

#if WITH_EDITOR
FString UPCGExPickerConstantSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Pick @");

	if (Config.bTreatAsNormalized)
	{
		DisplayName += FString::Printf(TEXT("%.2f"), Config.RelativeIndex);
	}
	else
	{
		DisplayName += FString::Printf(TEXT("%d"), Config.DiscreteIndex);
	}

	return DisplayName;
}
#endif

void UPCGExPickerConstantFactory::AddPicks(const int32 InNum, TSet<int32>& OutPicks) const
{
	int32 TargetIndex = 0;
	const int32 MaxIndex = InNum - 1;

	if (!Config.bTreatAsNormalized) { TargetIndex = Config.DiscreteIndex; }
	else { TargetIndex = PCGExMath::TruncateDbl(static_cast<double>(MaxIndex) * Config.RelativeIndex, Config.TruncateMode); }

	if (TargetIndex < 0) { TargetIndex = InNum + TargetIndex; }
	TargetIndex = PCGExMath::SanitizeIndex(TargetIndex, MaxIndex, Config.Safety);

	if (!FMath::IsWithin(TargetIndex, 0, InNum)) { return; }

	OutPicks.Add(TargetIndex);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

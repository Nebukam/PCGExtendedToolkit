// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerConstant.h"

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

bool UPCGExPickerConstant::Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

void UPCGExPickerConstant::AddPicks(const TSharedRef<PCGExData::FFacade>& InDataFacade, TSet<int32>& OutPicks) const
{
	int32 TargetIndex = 0;
	const int32 MaxIndex = InDataFacade->GetNum() - 1;

	if (!Config.bTreatAsNormalized) { TargetIndex = Config.DiscreteIndex; }
	else { TargetIndex = PCGEx::TruncateDbl(static_cast<double>(MaxIndex) * Config.RelativeIndex, Config.TruncateMode); }

	if (TargetIndex < 0) { TargetIndex = InDataFacade->GetNum() + TargetIndex; }
	TargetIndex = PCGExMath::SanitizeIndex(TargetIndex, MaxIndex, Config.Safety);

	if (!InDataFacade->GetIn()->GetPoints().IsValidIndex(TargetIndex)) { return; }

	OutPicks.Add(TargetIndex);
}

bool UPCGExPickerConstantFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

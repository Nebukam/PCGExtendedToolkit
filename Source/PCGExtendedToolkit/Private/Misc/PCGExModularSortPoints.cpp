// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExModularSortPoints.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

TArray<FPCGPinProperties> UPCGExModularSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, EPCGPinStatus::Required);
	return PinProperties;
}

bool UPCGExModularSortPointsSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::SourceSortingRules));
	return !OutRules.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

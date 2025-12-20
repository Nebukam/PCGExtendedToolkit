// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExCompareShorthandsDetails.h"

#include "Data/PCGExDataHelpers.h"
#include "Details/PCGExSettingsDetails.h"


PCGEX_SETTING_VALUE_IMPL(FPCGExCompareSelectorDouble, , double, Input, Attribute, Constant)

bool FPCGExCompareSelectorDouble::TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, double& OutValue, const bool bQuiet) const
{
	return PCGExData::Helpers::TryGetSettingDataValue(IO, Input, Attribute, Constant, OutValue, bQuiet);
}

#if WITH_EDITOR
FString FPCGExCompareSelectorDouble::GetDisplayNamePostfix() const
{
	FString DisplayName = PCGExCompare::ToString(Comparison);
	if (Input == EPCGExInputValueType::Attribute) { DisplayName += PCGExMetaHelpers::GetSelectorDisplayName(Attribute); }
	else { DisplayName += FString::Printf(TEXT("%.1f"), Constant); }
	return DisplayName;
}
#endif

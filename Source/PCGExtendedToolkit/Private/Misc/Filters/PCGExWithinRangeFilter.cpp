﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExWithinRangeFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Misc/Pickers/PCGExPickerAttributeSetRanges.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExWithinRangeFilterFactory::DomainCheck()
{
	return PCGExHelpers::IsDataDomainAttribute(Config.OperandA);
}

bool UPCGExWithinRangeFilterFactory::Init(FPCGExContext* InContext)
{
	if (Config.Source == EPCGExRangeSource::AttributeSet)
	{
		FPCGExPickerAttributeSetRangesConfig DummyConfig;
		DummyConfig.Attributes = Config.Attributes;
		if (!UPCGExPickerAttributeSetRangesFactory::GetUniqueRanges(InContext, FName("Ranges"), DummyConfig, Ranges)) { return false; }
	}
	else
	{
		FPCGExPickerConstantRangeConfig& Range = Ranges.Emplace_GetRef();
		Range.RelativeStartIndex = Config.RangeMin;
		Range.RelativeEndIndex = Config.RangeMax;
		Range.Sanitize();
	}

	return Super::Init(InContext);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExWithinRangeFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FWithinRangeFilter>(this);
}

TArray<FPCGPinProperties> UPCGExWithinRangeFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (Config.Source == EPCGExRangeSource::AttributeSet) { PCGEX_PIN_ANY(FName("Ranges"), "Data to read attribute ranges from", Required, {}) }
	else { PCGEX_PIN_ANY(FName("Ranges"), "Data to read attribute ranges from", Advanced, {}) }

	return PinProperties;
}

bool PCGExPointFilter::FWithinRangeFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandA, true);

	if (!OperandA)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, Operand A, TypedFilterFactory->Config.OperandA)
		return false;
	}

	bInclusive = TypedFilterFactory->Config.bInclusive;
	bInvert = TypedFilterFactory->Config.bInvert;

	return true;
}

bool PCGExPointFilter::FWithinRangeFilter::Test(const int32 PointIndex) const
{
	const double A = OperandA->Read(PointIndex);

	if (bInclusive)
	{
		for (const FPCGExPickerConstantRangeConfig& Range : Ranges) { if (Range.IsWithinInclusive(A)) { return !bInvert; } }
		return bInvert;
	}
	for (const FPCGExPickerConstantRangeConfig& Range : Ranges) { if (Range.IsWithin(A)) { return !bInvert; } }
	return bInvert;
}

bool PCGExPointFilter::FWithinRangeFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	double A = 0;
	if (!PCGExDataHelpers::TryReadDataValue(IO, TypedFilterFactory->Config.OperandA, A)) { return false; }

	if (bInclusive)
	{
		for (const FPCGExPickerConstantRangeConfig& Range : Ranges) { if (Range.IsWithinInclusive(A)) { return !bInvert; } }
		return bInvert;
	}
	for (const FPCGExPickerConstantRangeConfig& Range : Ranges) { if (Range.IsWithin(A)) { return !bInvert; } }
	return bInvert;
}

bool UPCGExWithinRangeFilterProviderSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == FName("Ranges")) { return Config.Source == EPCGExRangeSource::AttributeSet; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

PCGEX_CREATE_FILTER_FACTORY(WithinRange)

#if WITH_EDITOR
FString UPCGExWithinRangeFilterProviderSettings::GetDisplayName() const
{
	if (Config.Source == EPCGExRangeSource::AttributeSet)
	{
		return GetDefaultNodeTitle().ToString();
	}

	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + TEXT("[");

	DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.RangeMin) / 1000.0));
	DisplayName += TEXT(" .. ");
	DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.RangeMax) / 1000.0));

	return DisplayName + TEXT("]");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExModuloCompareFilter.h"

#include "PCGExHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Details/PCGExDetailsSettings.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExModuloCompareFilterConfig, OperandB, double, OperandBSource, OperandB, OperandBConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExModuloCompareFilterConfig, OperandC, double, CompareAgainst, OperandC, OperandCConstant)

bool UPCGExModuloCompareFilterFactory::DomainCheck()
{
	return
		PCGExHelpers::IsDataDomainAttribute(Config.OperandA) &&
		(Config.OperandBSource == EPCGExInputValueType::Constant || PCGExHelpers::IsDataDomainAttribute(Config.OperandB)) &&
		(Config.CompareAgainst == EPCGExInputValueType::Constant || PCGExHelpers::IsDataDomainAttribute(Config.OperandC));
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExModuloCompareFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FModuloComparisonFilter>(this);
}

void UPCGExModuloCompareFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	FacadePreloader.Register<double>(InContext, Config.OperandA);
	if (Config.OperandBSource == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandB); }
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandC); }
}

bool UPCGExModuloCompareFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.OperandBSource == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandC, Consumable)

	return true;
}

bool PCGExPointFilter::FModuloComparisonFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandA, true);

	if (!OperandA)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, Operand A, TypedFilterFactory->Config.OperandA)
		return false;
	}

	OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
	if (!OperandB->Init(PointDataFacade)) { return false; }

	OperandC = TypedFilterFactory->Config.GetValueSettingOperandC();
	if (!OperandC->Init(PointDataFacade)) { return false; }

	return true;
}

bool PCGExPointFilter::FModuloComparisonFilter::Test(const int32 PointIndex) const
{
	const double A = OperandA->Read(PointIndex);
	const double B = OperandB->Read(PointIndex);
	const double C = OperandC->Read(PointIndex);
	if (A == 0 || B == 0) { return TypedFilterFactory->Config.ZeroResult; }
	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Fmod(A, B), C, TypedFilterFactory->Config.Tolerance);
}

bool PCGExPointFilter::FModuloComparisonFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	double A = 0;
	double B = 0;
	double C = 0;

	if (!PCGExDataHelpers::TryReadDataValue(IO, TypedFilterFactory->Config.OperandA, A)) { return false; }
	if (!PCGExDataHelpers::TryGetSettingDataValue(IO, TypedFilterFactory->Config.OperandBSource, TypedFilterFactory->Config.OperandB, TypedFilterFactory->Config.OperandBConstant, B)) { return false; }
	if (!PCGExDataHelpers::TryGetSettingDataValue(IO, TypedFilterFactory->Config.CompareAgainst, TypedFilterFactory->Config.OperandC, TypedFilterFactory->Config.OperandCConstant, C)) { return false; }

	if (A == 0 || B == 0) { return TypedFilterFactory->Config.ZeroResult; }
	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Fmod(A, B), C, TypedFilterFactory->Config.Tolerance);
}

PCGEX_CREATE_FILTER_FACTORY(ModuloCompare)

#if WITH_EDITOR
FString UPCGExModuloCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + " % ";

	if (Config.OperandBSource == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f "), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	DisplayName += PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandC); }
	else { DisplayName += FString::Printf(TEXT(" %.3f"), (static_cast<int32>(1000 * Config.OperandCConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

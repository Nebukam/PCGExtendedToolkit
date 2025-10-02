﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"


#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExDotFilterConfig, OperandB, FVector, CompareAgainst, OperandB, OperandBConstant)

bool UPCGExDotFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

bool UPCGExDotFilterFactory::DomainCheck()
{
	return
		PCGExHelpers::IsDataDomainAttribute(Config.OperandA) &&
		(Config.CompareAgainst == EPCGExInputValueType::Constant || PCGExHelpers::IsDataDomainAttribute(Config.OperandB)) &&
		Config.DotComparisonDetails.GetOnlyUseDataDomain() &&
		!Config.bTransformOperandA && !Config.bTransformOperandB;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExDotFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FDotFilter>(this);
}

void UPCGExDotFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	FacadePreloader.Register<FVector>(InContext, Config.OperandA);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, Config.OperandB); }
	Config.DotComparisonDetails.RegisterBuffersDependencies(InContext, FacadePreloader);
}

bool UPCGExDotFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)
	Config.DotComparisonDetails.RegisterConsumableAttributesWithData(InContext, InData);

	return true;
}

bool PCGExPointFilter::FDotFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
	if (!DotComparison.Init(InContext, InPointDataFacade.ToSharedRef())) { return false; }


	OperandA = PointDataFacade->GetBroadcaster<FVector>(TypedFilterFactory->Config.OperandA, true);
	OperandAMultiplier = TypedFilterFactory->Config.bInvertOperandA ? -1 : 1;
	if (!OperandA)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, Operand A, TypedFilterFactory->Config.OperandA)
		return false;
	}

	OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
	if (!OperandB->Init(PointDataFacade)) { return false; }
	if (!OperandB->IsConstant()) { OperandBMultiplier = TypedFilterFactory->Config.bInvertOperandB ? -1 : 1; }

	InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

	return true;
}

bool PCGExPointFilter::FDotFilter::Test(const int32 PointIndex) const
{
	const FVector B = OperandB->Read(PointIndex).GetSafeNormal() * OperandBMultiplier;
	return DotComparison.Test(
		FVector::DotProduct(
			TypedFilterFactory->Config.bTransformOperandA ? InTransforms[PointIndex].TransformVectorNoScale(OperandA->Read(PointIndex) * OperandAMultiplier) : OperandA->Read(PointIndex) * OperandAMultiplier,
			TypedFilterFactory->Config.bTransformOperandB ? InTransforms[PointIndex].TransformVectorNoScale(B) : B),
		PointIndex);
}

bool PCGExPointFilter::FDotFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	PCGEX_SHARED_CONTEXT(IO->GetContextHandle())

	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;

	if (!PCGExDataHelpers::TryGetSettingDataValue(IO, TypedFilterFactory->Config.CompareAgainst, TypedFilterFactory->Config.OperandB, TypedFilterFactory->Config.OperandBConstant, B)) { return false; }
	B = B.GetSafeNormal();

	if (!PCGExDataHelpers::TryReadDataValue(IO, TypedFilterFactory->Config.OperandA, A)) { return false; }
	A = A.GetSafeNormal();

	FPCGExDotComparisonDetails TempComparison = TypedFilterFactory->Config.DotComparisonDetails;
	PCGEX_MAKE_SHARED(TempFacade, PCGExData::FFacade, IO.ToSharedRef())
	if (!TempComparison.Init(SharedContext.Get(), TempFacade.ToSharedRef()))
	{
		return false;
	}

	return TempComparison.Test(FVector::DotProduct(A, B), 0);
}

PCGEX_CREATE_FILTER_FACTORY(Dot)

#if WITH_EDITOR
FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + TEXT(" ⋅ ");

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += " (v3) "; }

	return DisplayName + Config.DotComparisonDetails.GetDisplayComparison();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

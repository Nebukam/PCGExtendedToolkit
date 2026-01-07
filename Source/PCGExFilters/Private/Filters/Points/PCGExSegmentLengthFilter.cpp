// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExSegmentLengthFilter.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExSegmentLengthFilterDefinition"
#define PCGEX_NAMESPACE PCGExSegmentLengthFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExSegmentLengthFilterConfig, Threshold, double, ThresholdInput, ThresholdAttribute, ThresholdConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExSegmentLengthFilterConfig, Index, int32, CompareAgainst, IndexAttribute, IndexConstant)

bool UPCGExSegmentLengthFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

bool UPCGExSegmentLengthFilterFactory::DomainCheck()
{
	//TODO
	return false;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExSegmentLengthFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FSegmentLengthFilter>(this);
}

void UPCGExSegmentLengthFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.ThresholdInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.ThresholdAttribute); }
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.IndexAttribute); }
}

bool UPCGExSegmentLengthFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.ThresholdInput == EPCGExInputValueType::Attribute, Config.ThresholdAttribute, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.IndexAttribute, Consumable)

	return true;
}

bool PCGExPointFilter::FSegmentLengthFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(InPointDataFacade->GetIn());
	LastIndex = InPointDataFacade->GetNum() - 1;
	InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();
	bOffset = TypedFilterFactory->Config.IndexMode == EPCGExIndexMode::Offset;

	if (TypedFilterFactory->Config.bForceTileIfClosedLoop && bClosedLoop) { IndexSafety = EPCGExIndexSafety::Tile; }
	else { IndexSafety = TypedFilterFactory->Config.IndexSafety; }

	Threshold = TypedFilterFactory->Config.GetValueSettingThreshold(PCGEX_QUIET_HANDLING);
	if (!Threshold->Init(PointDataFacade)) { return false; }

	Index = TypedFilterFactory->Config.GetValueSettingIndex(PCGEX_QUIET_HANDLING);
	if (!Index->Init(PointDataFacade)) { return false; }

	return true;
}

bool PCGExPointFilter::FSegmentLengthFilter::Test(const int32 PointIndex) const
{
	const int32 IndexValue = Index->Read(PointIndex);
	const int32 TargetIndex = PCGExMath::SanitizeIndex(bOffset ? PointIndex + IndexValue : IndexValue, LastIndex, IndexSafety);

	bool bResult = true;
	if (TargetIndex == -1)
	{
		bResult = TypedFilterFactory->Config.InvalidPointFallback != EPCGExFilterFallback::Fail;
	}
	else
	{
		bResult = PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, TypedFilterFactory->Config.bCompareAgainstSquaredDistance ? FVector::DistSquared(InTransforms[TargetIndex].GetLocation(), InTransforms[PointIndex].GetLocation()) : FVector::Dist(InTransforms[TargetIndex].GetLocation(), InTransforms[PointIndex].GetLocation()), Threshold->Read(PointIndex), TypedFilterFactory->Config.Tolerance);
	}

	return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
}

PCGEX_CREATE_FILTER_FACTORY(SegmentLength)

#if WITH_EDITOR
FString UPCGExSegmentLengthFilterProviderSettings::GetDisplayName() const
{
	FString TargetStr = Config.CompareAgainst == EPCGExInputValueType::Attribute ? PCGExMetaHelpers::GetSelectorDisplayName(Config.IndexAttribute) : FString::Printf(TEXT("%d"), Config.IndexConstant);
	FString OtherStr = Config.ThresholdInput == EPCGExInputValueType::Attribute ? PCGExMetaHelpers::GetSelectorDisplayName(Config.ThresholdAttribute) : FString::Printf(TEXT("%.1f"), Config.ThresholdConstant);
	FString Str = TEXT("Dist to ") + TargetStr + PCGExCompare::ToString(Config.Comparison) + OtherStr;
	return Str;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

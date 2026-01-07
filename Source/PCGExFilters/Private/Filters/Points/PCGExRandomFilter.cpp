// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExRandomFilter.h"

#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExRandomHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExRandomFilterConfig, Threshold, double, ThresholdInput, ThresholdAttribute, Threshold)
PCGEX_SETTING_VALUE_IMPL(FPCGExRandomFilterConfig, Weight, double, bPerPointWeight ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, Weight, 1)

bool UPCGExRandomFilterFactory::Init(FPCGExContext* InContext)
{
	Config.WeightLUT = Config.WeightCurveLookup.MakeLookup(Config.bUseLocalCurve, Config.LocalWeightCurve, Config.WeightCurve);
	return Super::Init(InContext);
}

bool UPCGExRandomFilterFactory::SupportsCollectionEvaluation() const
{
	return (!Config.bPerPointWeight && Config.ThresholdInput == EPCGExInputValueType::Constant) || bOnlyUseDataDomain;
}

bool UPCGExRandomFilterFactory::SupportsProxyEvaluation() const
{
	return !Config.bPerPointWeight && Config.ThresholdInput == EPCGExInputValueType::Constant;
}

void UPCGExRandomFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.bPerPointWeight && Config.bRemapWeightInternally) { FacadePreloader.Register<double>(InContext, Config.Weight); }
	if (Config.ThresholdInput != EPCGExInputValueType::Constant && Config.bRemapThresholdInternally) { FacadePreloader.Register<double>(InContext, Config.ThresholdAttribute); }
}

void UPCGExRandomFilterFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	InContext->AddAssetDependency(Config.WeightCurve.ToSoftObjectPath());
}

bool UPCGExRandomFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.bPerPointWeight, Config.Weight, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.ThresholdInput == EPCGExInputValueType::Attribute, Config.ThresholdAttribute, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExRandomFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FRandomFilter, this)
	Filter->WeightCurve = Config.WeightLUT;
	return Filter;
}

bool PCGExPointFilter::FRandomFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	Threshold = TypedFilterFactory->Config.Threshold;

	WeightBuffer = TypedFilterFactory->Config.GetValueSettingWeight(PCGEX_QUIET_HANDLING);
	if (!WeightBuffer->IsConstant())
	{
		if (TypedFilterFactory->Config.bRemapWeightInternally)
		{
			if (!WeightBuffer->Init(PointDataFacade, false, true)) { return false; }
			WeightRange = WeightBuffer->Max();

			if (WeightBuffer->Min() < 0)
			{
				WeightOffset = WeightBuffer->Min();
				WeightRange += WeightOffset;
			}
		}
		else
		{
			if (!WeightBuffer->Init(PointDataFacade)) { return false; }
		}
	}

	ThresholdBuffer = TypedFilterFactory->Config.GetValueSettingThreshold(PCGEX_QUIET_HANDLING);
	if (!ThresholdBuffer->IsConstant())
	{
		if (TypedFilterFactory->Config.bRemapThresholdInternally)
		{
			if (!ThresholdBuffer->Init(PointDataFacade, false, true)) { return false; }
			ThresholdRange = ThresholdBuffer->Max();

			if (ThresholdBuffer->Min() < 0)
			{
				ThresholdOffset = ThresholdBuffer->Min();
				ThresholdRange += ThresholdOffset;
			}
		}
		else
		{
			if (!ThresholdBuffer->Init(PointDataFacade)) { return false; }
		}
	}

	Seeds = PointDataFacade->GetIn()->GetConstSeedValueRange();

	RandomSeedV = FVector(RandomSeed);

	return true;
}

bool PCGExPointFilter::FRandomFilter::Test(const int32 PointIndex) const
{
	const double LocalWeightRange = WeightOffset + WeightBuffer->Read(PointIndex);
	const double LocalThreshold = ThresholdBuffer ? (ThresholdOffset + ThresholdBuffer->Read(PointIndex)) / ThresholdRange : Threshold;
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandomHelpers::GetRandomStreamFromPoint(Seeds[PointIndex], RandomSeed)).GetFraction() * LocalWeightRange) / WeightRange);
	return TypedFilterFactory->Config.bInvertResult ? RandomValue <= LocalThreshold : RandomValue >= LocalThreshold;
}

bool PCGExPointFilter::FRandomFilter::Test(const PCGExData::FProxyPoint& Point) const
{
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandomHelpers::ComputeSpatialSeed(Point.GetLocation(), RandomSeedV)).GetFraction() * WeightRange) / WeightRange);
	return TypedFilterFactory->Config.bInvertResult ? RandomValue <= Threshold : RandomValue >= Threshold;
}

bool PCGExPointFilter::FRandomFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandomHelpers::GetRandomStreamFromPoint(IO->GetIn()->GetSeed(0), RandomSeed)).GetFraction() * WeightRange) / WeightRange);
	return TypedFilterFactory->Config.bInvertResult ? RandomValue <= Threshold : RandomValue >= Threshold;
}

PCGEX_CREATE_FILTER_FACTORY(Random)

#if WITH_EDITOR
FString UPCGExRandomFilterProviderSettings::GetDisplayName() const
{
	return TEXT("Random");
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

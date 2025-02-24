// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExRandomFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExRandomFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Config.bUseLocalCurve) { Config.LocalWeightCurve.ExternalCurve = Config.WeightCurve.Get(); }
	return Super::Init(InContext);
}

bool UPCGExRandomFilterFactory::SupportsCollectionEvaluation() const
{
	return !Config.bPerPointWeight && Config.ThresholdInput == EPCGExInputValueType::Constant;
}

bool UPCGExRandomFilterFactory::SupportsDirectEvaluation() const
{
	return SupportsCollectionEvaluation();
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

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExRandomFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FRandomFilter, this)
	Filter->WeightCurve = Config.LocalWeightCurve.GetRichCurveConst();
	return Filter;
}

bool PCGExPointFilter::FRandomFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	Threshold = TypedFilterFactory->Config.Threshold;

	if (TypedFilterFactory->Config.bPerPointWeight)
	{
		if (TypedFilterFactory->Config.bRemapWeightInternally)
		{
			WeightBuffer = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.Weight, true);
			WeightRange = WeightBuffer->Max;

			if (WeightBuffer->Min < 0)
			{
				WeightOffset = WeightBuffer->Min;
				WeightRange += WeightOffset;
			}
		}
		else
		{
			WeightBuffer = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.Weight);
		}

		if (!WeightBuffer)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Weight", TypedFilterFactory->Config.Weight)
			return false;
		}
	}

	if (TypedFilterFactory->Config.ThresholdInput == EPCGExInputValueType::Attribute)
	{
		if (TypedFilterFactory->Config.bRemapThresholdInternally)
		{
			ThresholdBuffer = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.ThresholdAttribute, true);
			ThresholdRange = ThresholdBuffer->Max;

			if (ThresholdBuffer->Min < 0)
			{
				ThresholdOffset = ThresholdBuffer->Min;
				ThresholdRange += ThresholdOffset;
			}
		}
		else
		{
			ThresholdBuffer = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.ThresholdAttribute);
		}

		if (!ThresholdBuffer)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Threshold", TypedFilterFactory->Config.ThresholdAttribute)
			return false;
		}
	}

	return true;
}

bool PCGExPointFilter::FRandomFilter::Test(const int32 PointIndex) const
{
	const double LocalWeightRange = WeightBuffer ? WeightOffset + WeightBuffer->Read(PointIndex) : WeightRange;
	const double LocalThreshold = ThresholdBuffer ? (ThresholdOffset + ThresholdBuffer->Read(PointIndex)) / ThresholdRange : Threshold;
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandom::GetRandomStreamFromPoint(PointDataFacade->Source->GetInPoint(PointIndex), RandomSeed)).GetFraction() * LocalWeightRange) / WeightRange);
	return TypedFilterFactory->Config.bInvertResult ? RandomValue <= LocalThreshold : RandomValue >= LocalThreshold;
}

bool PCGExPointFilter::FRandomFilter::Test(const FPCGPoint& Point) const
{
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandom::GetRandomStreamFromPoint(Point, RandomSeed)).GetFraction() * WeightRange) / WeightRange);
	return TypedFilterFactory->Config.bInvertResult ? RandomValue <= Threshold : RandomValue >= Threshold;
}

bool PCGExPointFilter::FRandomFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	const float RandomValue = WeightCurve->Eval((FRandomStream(PCGExRandom::GetRandomStreamFromPoint(IO->GetInPoint(0), RandomSeed)).GetFraction() * WeightRange) / WeightRange);
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

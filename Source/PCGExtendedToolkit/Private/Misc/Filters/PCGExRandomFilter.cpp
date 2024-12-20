// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExRandomFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExRandomFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Config.bUseLocalCurve) { Config.LocalWeightCurve.ExternalCurve = Config.WeightCurve.Get(); }
	return Super::Init(InContext);
}

void UPCGExRandomFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.bPerPointWeight && !Config.bRemapWeightInternally) { FacadePreloader.Register<double>(InContext, Config.Weight); }
}

void UPCGExRandomFilterFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	InContext->AddAssetDependency(Config.WeightCurve.ToSoftObjectPath());
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExRandomFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointsFilter::TRandomFilter, this)
	Filter->WeightCurve = Config.LocalWeightCurve.GetRichCurveConst();
	return Filter;
}

bool PCGExPointsFilter::TRandomFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
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
				WeightOffset = FMath::Abs(WeightBuffer->Min);
				WeightRange += WeightOffset;
			}
		}
		else
		{
			WeightBuffer = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.Weight);
		}

		if (!WeightBuffer)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Weight attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.Weight.GetName())));
			return false;
		}
	}

	return true;
}

PCGEX_CREATE_FILTER_FACTORY(Random)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

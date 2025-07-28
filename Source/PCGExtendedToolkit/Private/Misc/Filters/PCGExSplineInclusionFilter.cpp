// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineInclusionFilter.h"


#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExSplineInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineInclusionFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExSplineInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FSplineInclusionFilter>(this);
}

void UPCGExSplineInclusionFilterFactory::InitConfig_Internal()
{
	Super::InitConfig_Internal();
	LocalFidelity = Config.Fidelity;
	LocalExpansion = Config.Tolerance;
	LocalExpansionZ = -1;
	LocalProjection = Config.ProjectionDetails;
	LocalSampleInputs = Config.SampleInputs;
	WindingMutation = Config.WindingMutation;
	bScaleTolerance = Config.bSplineScalesTolerance;
}

namespace PCGExPointFilter
{
	bool FSplineInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

		bCheckAgainstDataBounds = TypedFilterFactory->Config.bCheckAgainstDataBounds;
		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

		if (bCheckAgainstDataBounds)
		{
			PCGExData::FProxyPoint ProxyPoint;
			InPointDataFacade->Source->GetDataAsProxyPoint(ProxyPoint);
			bCollectionTestResult = Test(ProxyPoint);
		}

		return true;
	}

#define PCGEX_CHECK_MAX if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
#define PCGEX_CHECK_MIN if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }

	bool FSplineInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		int32 InclusionsCount = 0;
		PCGExPathInclusion::EFlags Flags = Handler->GetInclusionFlags(
			Point.GetLocation(), InclusionsCount,
			TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest);

		PCGEX_CHECK_MAX
		PCGEX_CHECK_MIN

		const bool bPass = Handler->TestFlags(Flags);
		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FSplineInclusionFilter::Test(const int32 PointIndex) const
	{
		if (bCheckAgainstDataBounds) { return bCollectionTestResult; }

		int32 InclusionsCount = 0;
		PCGExPathInclusion::EFlags Flags = Handler->GetInclusionFlags(
			InTransforms[PointIndex].GetLocation(), InclusionsCount,
			TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest);

		PCGEX_CHECK_MAX
		PCGEX_CHECK_MIN

		const bool bPass = Handler->TestFlags(Flags);
		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FSplineInclusionFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
	{
		PCGExData::FProxyPoint ProxyPoint;
		IO->GetDataAsProxyPoint(ProxyPoint);
		return Test(ProxyPoint);
	}

#undef PCGEX_CHECK_MAX
#undef PCGEX_CHECK_MIN
}

TArray<FPCGPinProperties> UPCGExSplineInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(SplineInclusion)

#if WITH_EDITOR
FString UPCGExSplineInclusionFilterProviderSettings::GetDisplayName() const
{
	return PCGExPathInclusion::ToString(Config.CheckType);
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExInclusionFilter.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Paths/PCGExPath.h"
#include "PCGExVersion.h"


#define LOCTEXT_NAMESPACE "PCGExInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExInclusionFilterDefinition

bool UPCGExInclusionFilterFactory::SupportsCollectionEvaluation() const
{
	return Config.bCheckAgainstDataBounds;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FInclusionFilter>(this);
}

FName UPCGExInclusionFilterFactory::GetInputLabel() const
{
	return PCGExCommon::Labels::SourceTargetsLabel;
}

void UPCGExInclusionFilterFactory::InitConfig_Internal()
{
	Super::InitConfig_Internal();
	LocalFidelity = Config.Fidelity;
	LocalExpansion = Config.Tolerance;
	LocalExpansionZ = Config.ExpandZAxis;
	InclusionOffset = Config.InclusionOffset;
	LocalProjection = Config.ProjectionDetails;
	LocalSampleInputs = Config.SampleInputs;
	WindingMutation = Config.WindingMutation;
	bScaleTolerance = Config.bSplineScalesTolerance;
	bIgnoreSelf = Config.bIgnoreSelf;
}

namespace PCGExPointFilter
{
	bool FInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
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

	bool FInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		int32 InclusionsCount = 0;
		PCGExPathInclusion::EFlags Flags = Handler->GetInclusionFlags(Point.GetLocation(), InclusionsCount, TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest);

		PCGEX_CHECK_MAX
		PCGEX_CHECK_MIN

		const bool bPass = Handler->TestFlags(Flags);
		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FInclusionFilter::Test(const int32 PointIndex) const
	{
		if (bCheckAgainstDataBounds) { return bCollectionTestResult; }

		int32 InclusionsCount = 0;
		PCGExPathInclusion::EFlags Flags = Handler->GetInclusionFlags(InTransforms[PointIndex].GetLocation(), InclusionsCount, TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest, PointDataFacade->Source->GetIn());

		PCGEX_CHECK_MAX
		PCGEX_CHECK_MIN

		const bool bPass = Handler->TestFlags(Flags);
		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FInclusionFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
	{
		PCGExData::FProxyPoint ProxyPoint;
		IO->GetDataAsProxyPoint(ProxyPoint);

		int32 InclusionsCount = 0;
		PCGExPathInclusion::EFlags Flags = Handler->GetInclusionFlags(ProxyPoint.GetLocation(), InclusionsCount, TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest, IO->GetInOut());

		PCGEX_CHECK_MAX
		PCGEX_CHECK_MIN

		const bool bPass = Handler->TestFlags(Flags);
		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

#undef PCGEX_CHECK_MAX
#undef PCGEX_CHECK_MIN
}

TArray<FPCGPinProperties> UPCGExInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExPathInclusion::DeclareInclusionPin(PinProperties);
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Inclusion)

#if WITH_EDITOR
TArray<FPCGPreConfiguredSettingsInfo> UPCGExInclusionFilterProviderSettings::GetPreconfiguredInfo() const
{
	TArray<FPCGPreConfiguredSettingsInfo> Infos;

	const TSet<EPCGExSplineCheckType> ValuesToSkip = {};
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExSplineCheckType>(ValuesToSkip, FTEXT("{0}"));
}
#endif

void UPCGExInclusionFilterProviderSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	Super::ApplyPreconfiguredSettings(PreconfigureInfo);
	if (const UEnum* EnumPtr = StaticEnum<EPCGExSplineCheckType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Config.CheckType = static_cast<EPCGExSplineCheckType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

#if WITH_EDITOR
FString UPCGExInclusionFilterProviderSettings::GetDisplayName() const
{
	return PCGExPathInclusion::ToString(Config.CheckType);
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

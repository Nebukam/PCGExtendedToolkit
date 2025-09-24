// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBoundsFilter.h"

#include "Data/PCGExData.h"
#include "Geometry/PCGExGeoPointBox.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsFilterDefinition"
#define PCGEX_NAMESPACE PCGExBoundsFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExBoundsFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FBoundsFilter>(this);
}

PCGExFactories::EPreparationResult UPCGExBoundsFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	if (!PCGExData::TryGetFacades(InContext, FName("Bounds"), BoundsDataFacades, false))
	{
		if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing bounds data.")); } }
		return PCGExFactories::EPreparationResult::MissingData;
	}

	Clouds.Reserve(BoundsDataFacades.Num());
	for (const TSharedPtr<PCGExData::FFacade>& Facade : BoundsDataFacades)
	{
		Clouds.Add(
			Facade->GetCloud(
				Config.BoundsTarget,
				Config.TestMode == EPCGExBoxCheckMode::ExpandedBox || Config.TestMode == EPCGExBoxCheckMode::ExpandedSphere ? Config.Expansion * 2 : Config.Expansion));
	}

	return Result;
}

void UPCGExBoundsFilterFactory::BeginDestroy()
{
	BoundsDataFacades.Empty();
	Clouds.Empty();
	Super::BeginDestroy();
}

bool PCGExPointFilter::FBoundsFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
	if (!Clouds) { return false; }

	bCheckAgainstDataBounds = TypedFilterFactory->Config.bCheckAgainstDataBounds;
	BoundsTarget = TypedFilterFactory->Config.BoundsTarget;

#define PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST:\
	BoundCheck = [&](const PCGExData::FConstPoint& Point) { if (bCheckAgainstDataBounds) { return bCollectionTestResult; } for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;};\
	BoundCheckProxy = [&](const PCGExData::FProxyPoint& Point) { for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;};\
	break;
#define PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST:\
	BoundCheck = [&](const PCGExData::FConstPoint& Point) { if (bCheckAgainstDataBounds) { return bCollectionTestResult; } for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(!Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;};\
	BoundCheckProxy = [&](const PCGExData::FProxyPoint& Point) { for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(!Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;};\
	break;
#define PCGEX_FOREACH_TESTTYPE(_NAME, _BOUNDS)\
case EPCGExPointBoundsSource::_BOUNDS:\
switch (TypedFilterFactory->Config.TestMode) { default: \
PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, Box)\
PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, ExpandedBox)\
PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, Sphere)\
PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, ExpandedSphere) }\
break;

#define PCGEX_FOREACH_TESTTYPE_INV(_NAME, _BOUNDS)\
case EPCGExPointBoundsSource::_BOUNDS:\
switch (TypedFilterFactory->Config.TestMode) { default: \
PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, Box)\
PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, ExpandedBox)\
PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, Sphere)\
PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, ExpandedSphere) }\
break;

#define PCGEX_FOREACH_BOUNDTYPE(_NAME)\
if(TypedFilterFactory->Config.bInvert){\
	switch (TypedFilterFactory->Config.BoundsSource) { default: \
	PCGEX_FOREACH_TESTTYPE_INV(_NAME, ScaledBounds)\
	PCGEX_FOREACH_TESTTYPE_INV(_NAME, DensityBounds)\
	PCGEX_FOREACH_TESTTYPE_INV(_NAME, Bounds)\
	PCGEX_FOREACH_TESTTYPE_INV(_NAME, Center)}\
	}else{\
	switch (TypedFilterFactory->Config.BoundsSource) { default: \
	PCGEX_FOREACH_TESTTYPE(_NAME, ScaledBounds)\
	PCGEX_FOREACH_TESTTYPE(_NAME, DensityBounds)\
	PCGEX_FOREACH_TESTTYPE(_NAME, Bounds)\
	PCGEX_FOREACH_TESTTYPE(_NAME, Center)}}

	if (TypedFilterFactory->Config.Mode == EPCGExBoundsFilterCompareMode::PerPointBounds)
	{
		switch (TypedFilterFactory->Config.CheckType)
		{
		default:
		case EPCGExBoundsCheckType::Intersects:
			PCGEX_FOREACH_BOUNDTYPE(Intersect)
			break;
		case EPCGExBoundsCheckType::IsInside:
			PCGEX_FOREACH_BOUNDTYPE(IsInside)
			break;
		case EPCGExBoundsCheckType::IsInsideOrOn:
			PCGEX_FOREACH_BOUNDTYPE(IsInsideOrOn)
			break;
		case EPCGExBoundsCheckType::IsInsideOrIntersects:
			PCGEX_FOREACH_BOUNDTYPE(IsInsideOrIntersects)
			break;
		}
	}
	else
	{
		switch (TypedFilterFactory->Config.CheckType)
		{
		default:
		case EPCGExBoundsCheckType::Intersects:
			PCGEX_FOREACH_BOUNDTYPE(IntersectCloud)
			break;
		case EPCGExBoundsCheckType::IsInside:
			PCGEX_FOREACH_BOUNDTYPE(IsInsideCloud)
			break;
		case EPCGExBoundsCheckType::IsInsideOrOn:
			PCGEX_FOREACH_BOUNDTYPE(IsInsideOrOnCloud)
			break;
		case EPCGExBoundsCheckType::IsInsideOrIntersects:
			PCGEX_FOREACH_BOUNDTYPE(IsInsideOrIntersectsCloud)
			break;
		}
	}

#undef PCGEX_TEST_BOUNDS
#undef PCGEX_TEST_BOUNDS_INV
#undef PCGEX_FOREACH_TESTTYPE
#undef PCGEX_FOREACH_TESTTYPE_INV
#undef PCGEX_FOREACH_BOUNDTYPE

	if (bCheckAgainstDataBounds)
	{
		PCGExData::FProxyPoint ProxyPoint;
		InPointDataFacade->Source->GetDataAsProxyPoint(ProxyPoint);
		bCollectionTestResult = Test(ProxyPoint);
		return true;
	}

	return true;
}

bool PCGExPointFilter::FBoundsFilter::Test(const int32 PointIndex) const { return BoundCheck(PointDataFacade->Source->GetInPoint(PointIndex)); }

bool PCGExPointFilter::FBoundsFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	PCGExData::FProxyPoint ProxyPoint;
	IO->GetDataAsProxyPoint(ProxyPoint);
	return Test(ProxyPoint);
}

TArray<FPCGPinProperties> UPCGExBoundsFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(FName("Bounds"), TEXT("Points which bounds will be used for testing"), Required)
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Bounds)

#if WITH_EDITOR
FString UPCGExBoundsFilterProviderSettings::GetDisplayName() const
{
	switch (Config.CheckType)
	{
	default:
	case EPCGExBoundsCheckType::Intersects: return TEXT("Intersects");
	case EPCGExBoundsCheckType::IsInside: return TEXT("Is Inside");
	case EPCGExBoundsCheckType::IsInsideOrOn: return TEXT("Is Inside or On");
	case EPCGExBoundsCheckType::IsInsideOrIntersects: return TEXT("Is Inside or Intersects");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

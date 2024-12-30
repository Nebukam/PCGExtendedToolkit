// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBoundsFilter.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsFilterDefinition"
#define PCGEX_NAMESPACE PCGExBoundsFilterDefinition

bool UPCGExBoundsFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	BoundsDataFacade = PCGExData::TryGetSingleFacade(InContext, FName("Bounds"), true);
	if (!BoundsDataFacade) { return false; }

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExBoundsFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TBoundsFilter>(this);
}

void UPCGExBoundsFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

bool PCGExPointsFilter::TBoundsFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }
	if (!Cloud) { return false; }

	BoundsTarget = TypedFilterFactory->Config.BoundsTarget;

#define PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST: BoundCheck = [&](const FPCGPoint& Point) { return Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point); }; break;
#define PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST: BoundCheck = [&](const FPCGPoint& Point) { return !Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point); }; break;
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
	PCGEX_FOREACH_TESTTYPE_INV(_NAME, Bounds)}\
	}else{\
	switch (TypedFilterFactory->Config.BoundsSource) { default: \
	PCGEX_FOREACH_TESTTYPE(_NAME, ScaledBounds)\
	PCGEX_FOREACH_TESTTYPE(_NAME, DensityBounds)\
	PCGEX_FOREACH_TESTTYPE(_NAME, Bounds)}}

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

	return true;
}

TArray<FPCGPinProperties> UPCGExBoundsFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(FName("Bounds"), TEXT("Points which bounds will be used for testing"), Required, {})
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

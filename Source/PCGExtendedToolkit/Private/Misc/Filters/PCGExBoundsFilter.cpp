// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBoundsFilter.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsFilterDefinition"
#define PCGEX_NAMESPACE PCGExBoundsFilterDefinition

bool UPCGExBoundsFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	TSharedPtr<PCGExData::FPointIOCollection> PointIOCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, FName("Bounds"));
	if (PointIOCollection->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing bounds data.")); }
		return false;
	}

	BoundsDataFacades.Reserve(PointIOCollection->Num());
	Clouds.Reserve(PointIOCollection->Num());

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : PointIOCollection->Pairs)
	{
		PCGEX_MAKE_SHARED(NewFacade, PCGExData::FFacade, PointIO.ToSharedRef())
		BoundsDataFacades.Add(NewFacade);
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExBoundsFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::FBoundsFilter>(this);
}

bool UPCGExBoundsFilterFactory::Prepare(FPCGExContext* InContext)
{
	for (const TSharedPtr<PCGExData::FFacade>& Facade : BoundsDataFacades)
	{
		Clouds.Add(
			Facade->GetCloud(
				Config.BoundsTarget,
				Config.TestMode == EPCGExBoxCheckMode::ExpandedBox || Config.TestMode == EPCGExBoxCheckMode::ExpandedSphere ? Config.Expansion * 2 : Config.Expansion));
	}

	return Super::Prepare(InContext);
}

void UPCGExBoundsFilterFactory::BeginDestroy()
{
	BoundsDataFacades.Empty();
	Clouds.Empty();
	Super::BeginDestroy();
}

bool PCGExPointsFilter::FBoundsFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }
	if (!Clouds) { return false; }

	BoundsTarget = TypedFilterFactory->Config.BoundsTarget;

#define PCGEX_TEST_BOUNDS(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST: BoundCheck = [&](const FPCGPoint& Point) { for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;}; break;
#define PCGEX_TEST_BOUNDS_INV(_NAME, _BOUNDS, _TEST)\
case EPCGExBoxCheckMode::_TEST: BoundCheck = [&](const FPCGPoint& Point) { for(const TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud : *Clouds){ if(!Cloud->_NAME<EPCGExPointBoundsSource::_BOUNDS, EPCGExBoxCheckMode::_TEST>(Point)){return true;} } return false;}; break;
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
	PCGEX_PIN_POINTS(FName("Bounds"), TEXT("Points which bounds will be used for testing"), Required, {})
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

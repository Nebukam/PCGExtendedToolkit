// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBoundsFilter.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsFilterDefinition"
#define PCGEX_NAMESPACE PCGExBoundsFilterDefinition

bool UPCGExBoundsFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	if (const TSharedPtr<PCGExData::FPointIO> BoundsIO = PCGExData::TryGetSingleInput(InContext, FName("Bounds"), true))
	{
		BoundsDataFacade = MakeShared<PCGExData::FFacade>(BoundsIO.ToSharedRef());
	}
	else
	{
		return false;
	}

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

bool PCGExPointsFilter::TBoundsFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }
	if (!Cloud) { return false; }


	switch (TypedFilterFactory->Config.CheckType)
	{
	default:
	case EPCGExBoundsCheckType::Overlap:
		BoundCheck = [&](const FPCGPoint& Point) { return Cloud->ContainsMinusEpsilon(Point.Transform.GetLocation()); };
		break;
	case EPCGExBoundsCheckType::Inside:
		BoundCheck = [&](const FPCGPoint& Point) { return Cloud->ContainsMinusEpsilon(Point.Transform.GetLocation()); };
		break;
	case EPCGExBoundsCheckType::Outside:
		BoundCheck = [&](const FPCGPoint& Point) { return !Cloud->ContainsMinusEpsilon(Point.Transform.GetLocation()); };
		break;
	}

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
	case EPCGExBoundsCheckType::Overlap: return TEXT("Overlap");
	case EPCGExBoundsCheckType::Inside: return TEXT("Inside");
	case EPCGExBoundsCheckType::Outside: return TEXT("Outside");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

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

TSharedPtr<PCGExPointFilter::TFilter> UPCGExBoundsFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TBoundsFilter>(this);
}

void UPCGExBoundsFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

bool PCGExPointsFilter::TBoundsFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }
	return Cloud ? true : false;
}

bool PCGExPointsFilter::TBoundsFilter::Test(const int32 PointIndex) const
{
	return Cloud->ContainsMinusEpsilon(PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation()) ? TypedFilterFactory->Config.bCheckIfInside : !TypedFilterFactory->Config.bCheckIfInside;
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
	return Config.bCheckIfInside ? TEXT("Inside") : TEXT("Outside");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

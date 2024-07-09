// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBoundsFilter.h"

bool UPCGExBoundsFilterFactory::Init(const FPCGContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	if (PCGExData::FPointIO* BoundsIO = PCGExData::TryGetSingleInput(InContext, FName("Bounds"), true))
	{
		BoundsDataFacade = new PCGExData::FFacade(BoundsIO);
	}
	else
	{
		return false;
	}

	return true;
}

PCGExPointFilter::TFilter* UPCGExBoundsFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBoundsFilter(this);
}

void UPCGExBoundsFilterFactory::BeginDestroy()
{
	if (BoundsDataFacade) { PCGEX_DELETE(BoundsDataFacade->Source) }
	PCGEX_DELETE(BoundsDataFacade)
	Super::BeginDestroy();
}

bool PCGExPointsFilter::TBoundsFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }
	return Cloud ? true : false;
}

bool PCGExPointsFilter::TBoundsFilter::Test(const int32 PointIndex) const
{
	return Cloud->Overlaps(PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation()) ? TypedFilterFactory->Config.bCheckIfInside : !TypedFilterFactory->Config.bCheckIfInside;
}

#define LOCTEXT_NAMESPACE "PCGExBoundsFilterDefinition"
#define PCGEX_NAMESPACE PCGExBoundsFilterDefinition

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

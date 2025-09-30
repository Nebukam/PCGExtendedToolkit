﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExAngleFilter.h"

#include "PCGExHelpers.h"
#include "Data/PCGExData.h"
#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExAngleFilterDefinition"
#define PCGEX_NAMESPACE PCGExAngleFilterDefinition

bool UPCGExAngleFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

bool UPCGExAngleFilterFactory::DomainCheck()
{
	return Config.DotComparisonDetails.GetOnlyUseDataDomain();
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExAngleFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FAngleFilter>(this);
}

void UPCGExAngleFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	Config.DotComparisonDetails.RegisterBuffersDependencies(InContext, FacadePreloader);
}

bool UPCGExAngleFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }
	Config.DotComparisonDetails.RegisterConsumableAttributesWithData(InContext, InData);
	return true;
}

bool PCGExPointFilter::FAngleFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
	if (!DotComparison.Init(InContext, InPointDataFacade.ToSharedRef())) { return false; }

	bClosedLoop = PCGExPaths::GetClosedLoop(InPointDataFacade->GetIn());
	LastIndex = InPointDataFacade->GetNum() - 1;
	InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

	return true;
}

bool PCGExPointFilter::FAngleFilter::Test(const int32 PointIndex) const
{
	int32 PrevIndex = PointIndex - 1;
	int32 NextIndex = PointIndex + 1;

	bool bResult = !TypedFilterFactory->Config.bInvert;

	if (bClosedLoop)
	{
		if (PointIndex == 0) { PrevIndex = LastIndex; }
		else if (PointIndex == LastIndex) { NextIndex = 0; }
	}
	else
	{
		if (PointIndex == 0) { return TypedFilterFactory->Config.FirstPointFallback == EPCGExFilterFallback::Fail ? !bResult : bResult; }
		if (PointIndex == LastIndex) { return TypedFilterFactory->Config.LastPointFallback == EPCGExFilterFallback::Fail ? !bResult : bResult; }
	}

	const FVector Prev = InTransforms[PrevIndex].GetLocation();
	const FVector Corner = InTransforms[PointIndex].GetLocation();
	const FVector Next = InTransforms[NextIndex].GetLocation();

	if (TypedFilterFactory->Config.Mode == EPCGExAngleFilterMode::Curvature)
	{
		bResult = DotComparison.Test(FVector::DotProduct((Corner - Prev).GetSafeNormal(), (Next - Corner).GetSafeNormal()), PointIndex);
	}
	else
	{
		bResult = DotComparison.Test(FVector::DotProduct((Prev - Corner).GetSafeNormal(), (Next - Corner).GetSafeNormal()), PointIndex);
	}

	return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
}

PCGEX_CREATE_FILTER_FACTORY(Angle)

#if WITH_EDITOR
FString UPCGExAngleFilterProviderSettings::GetDisplayName() const
{
	return (Config.Mode == EPCGExAngleFilterMode::Curvature ? TEXT("Curvature") : TEXT("Spread")) + Config.DotComparisonDetails.GetDisplayComparison();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

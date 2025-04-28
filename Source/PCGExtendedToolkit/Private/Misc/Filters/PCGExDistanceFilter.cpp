// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDistanceFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExDistanceFilterFactory::SupportsDirectEvaluation() const
{
	return Config.CompareAgainst == EPCGExInputValueType::Constant;
}

bool UPCGExDistanceFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExDistanceFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FDistanceFilter>(this);
}

bool UPCGExDistanceFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.DistanceThreshold, Consumable)

	return true;
}

bool UPCGExDistanceFilterFactory::Prepare(FPCGExContext* InContext)
{
	TSharedPtr<PCGExData::FPointIOCollection> PointIOCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGEx::SourceTargetsLabel);
	if (PointIOCollection->IsEmpty()) { return false; }

	Octrees.Reserve(PointIOCollection->Num());
	TargetsPtr.Reserve(PointIOCollection->Num());

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : PointIOCollection->Pairs)
	{
		Octrees.Add(&PointIO->GetIn()->GetOctree());
		TargetsPtr.Add(reinterpret_cast<uintptr_t>(PointIO->GetIn()->GetPoints().GetData()));
	}

	return Super::Prepare(InContext);
}

void UPCGExDistanceFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

bool PCGExPointFilter::FDistanceFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	if (!Octrees) { return false; }

	NumTargets = Octrees->Num();

	SelfPtr = reinterpret_cast<uintptr_t>(InPointDataFacade->GetIn()->GetPoints().GetData());
	Distances = TypedFilterFactory->Config.DistanceDetails.MakeDistances();

	DistanceThresholdGetter = TypedFilterFactory->Config.GetValueSettingDistanceThreshold();
	if(!DistanceThresholdGetter->Init(InContext, InPointDataFacade)){return false;}
	
	return true;
}

bool PCGExPointFilter::FDistanceFilter::Test(const FPCGPoint& Point) const
{
	double BestDist = MAX_dbl;

	if (Distances->bOverlapIsZero)
	{
		bool bOverlap = false;
		for (int i = 0; i < NumTargets; i++)
		{
			const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);
			if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						double Dist = Distances->GetDistSquared(Point, *PointRef.Point, bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						double Dist = Distances->GetDistSquared(Point, *PointRef.Point, bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
		}
	}
	else
	{
		for (int i = 0; i < NumTargets; i++)
		{
			const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);

			if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						const double Dist = Distances->GetDistSquared(Point, *PointRef.Point);
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						const double Dist = Distances->GetDistSquared(Point, *PointRef.Point);
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
		}
	}

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Sqrt(BestDist), TypedFilterFactory->Config.DistanceThresholdConstant, TypedFilterFactory->Config.Tolerance);
}

bool PCGExPointFilter::FDistanceFilter::Test(const int32 PointIndex) const
{
	const double B = DistanceThresholdGetter->Read(PointIndex);

	const FPCGPoint& SourcePt = PointDataFacade->Source->GetInPoint(PointIndex);
	double BestDist = MAX_dbl;

	if (Distances->bOverlapIsZero)
	{
		bool bOverlap = false;
		for (int i = 0; i < NumTargets; i++)
		{
			const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);
			if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						if (const ptrdiff_t OtherIndex = PointRef.Point - PointDataFacade->GetIn()->GetPoints().GetData();
							OtherIndex == PointIndex) { return; }

						double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point, bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point, bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
		}
	}
	else
	{
		for (int i = 0; i < NumTargets; i++)
		{
			const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);

			if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						if (const ptrdiff_t OtherIndex = PointRef.Point - PointDataFacade->GetIn()->GetPoints().GetData();
							OtherIndex == PointIndex) { return; }

						const double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point);
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
					{
						const double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point);
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
		}
	}

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Sqrt(BestDist), B, TypedFilterFactory->Config.Tolerance);
}

TArray<FPCGPinProperties> UPCGExDistanceFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, TEXT("Target points to read operand B from"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Distance)

#if WITH_EDITOR
FString UPCGExDistanceFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Distance ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.DistanceThreshold); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.DistanceThresholdConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

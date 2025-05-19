// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDistanceFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExDistanceFilterFactory::SupportsPointEvaluation() const
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

	OctreesPtr.Reserve(PointIOCollection->Num());
	TargetsPtr.Reserve(PointIOCollection->Num());

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : PointIOCollection->Pairs)
	{
		OctreesPtr.Add(&PointIO->GetIn()->GetPointOctree());
		TargetsPtr.Add(&PointIO->GetIn()->GetPoints());
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

	if (OctreesPtr.IsEmpty()) { return false; }

	NumTargets = OctreesPtr.Num();

	SelfPtr = reinterpret_cast<uintptr_t>(InPointDataFacade->GetIn()->GetPoints().GetData());
	Distances = TypedFilterFactory->Config.DistanceDetails.MakeDistances();

	DistanceThresholdGetter = TypedFilterFactory->Config.GetValueSettingDistanceThreshold();
	if (!DistanceThresholdGetter->Init(InContext, InPointDataFacade)) { return false; }

	return true;
}

#define PCGEX_DIST_REINTERP_CAST \
const TArray<FPCGPoint>* TargetPoints = TargetsPtr[i]; \
const uintptr_t Current = reinterpret_cast<uintptr_t>(TargetPoints->GetData()); \
const PCGPointOctree::FPointOctree* TargetOctree = OctreesPtr[i];

#if PCGEX_ENGINE_VERSION < 506
#define PCGEX_POINTREF_INDEX const int32 OtherIndex = static_cast<int32>(PointRef.Point - TargetPoints->GetData());
#else
#define PCGEX_POINTREF_INDEX const int32 OtherIndex = PointRef.Index;
#endif

bool PCGExPointFilter::FDistanceFilter::TestRoamingPoint(const FPCGPoint& Point) const
{
	double BestDist = MAX_dbl;
	const FVector Origin = Point.Transform.GetLocation();

	if (Distances->bOverlapIsZero)
	{
		bool bOverlap = false;
		for (int i = 0; i < NumTargets; i++)
		{
			PCGEX_DIST_REINTERP_CAST

			if (Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						double Dist = Distances->GetDistSquared(Point, *(TargetPoints->GetData() + OtherIndex), bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						double Dist = Distances->GetDistSquared(Point, *(TargetPoints->GetData() + OtherIndex), bOverlap);
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
			PCGEX_DIST_REINTERP_CAST

			if (Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						const double Dist = Distances->GetDistSquared(Point, *(TargetPoints->GetData() + OtherIndex));
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						const double Dist = Distances->GetDistSquared(Point, *(TargetPoints->GetData() + OtherIndex));
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
	const FVector Origin = SourcePt.Transform.GetLocation();

	double BestDist = MAX_dbl;

	if (Distances->bOverlapIsZero)
	{
		bool bOverlap = false;
		for (int i = 0; i < NumTargets; i++)
		{
			PCGEX_DIST_REINTERP_CAST

			if (Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						if (OtherIndex == PointIndex) { return; }

						double Dist = Distances->GetDistSquared(SourcePt, *(TargetPoints->GetData() + OtherIndex), bOverlap);
						if (bOverlap) { Dist = 0; }
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						double Dist = Distances->GetDistSquared(SourcePt, *(TargetPoints->GetData() + OtherIndex), bOverlap);
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
			PCGEX_DIST_REINTERP_CAST

			if (Current == SelfPtr)
			{
				if (bIgnoreSelf) { continue; }

				// Ignore current point when testing against self
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						if (OtherIndex == PointIndex) { return; }

						const double Dist = Distances->GetDistSquared(SourcePt, *(TargetPoints->GetData() + OtherIndex));
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
			else
			{
				TargetOctree->FindNearbyElements(
					Origin, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGEX_POINTREF_INDEX
						const double Dist = Distances->GetDistSquared(SourcePt, *(TargetPoints->GetData() + OtherIndex));
						if (Dist > BestDist) { return; }
						BestDist = Dist;
					});
			}
		}
	}

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Sqrt(BestDist), B, TypedFilterFactory->Config.Tolerance);
}

#undef PCGEX_POINTREF_INDEX
#undef PCGEX_DIST_REINTERP_CAST

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

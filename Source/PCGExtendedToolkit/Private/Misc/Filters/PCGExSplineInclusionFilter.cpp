// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineInclusionFilter.h"


#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExSplineInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineInclusionFilterDefinition

bool UPCGExSplineInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExSplineInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FSplineInclusionFilter>(this);
}

bool UPCGExSplineInclusionFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExSplineInclusionFilterFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	Splines = MakeShared<TArray<FPCGSplineStruct>>();
	TArray<FBox> BoundsList;
	FBox OctreeBounds = FBox(ForceInit);

	if (Config.bTestInclusionOnProjection)
	{
		Polygons = MakeShared<TArray<TArray<FVector2D>>>();
		Projections = MakeShared<TArray<FQuat>>();
	}

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
		!Targets.IsEmpty())
	{
		TArray<FVector> SplinePoints;
		BoundsList.Reserve(Targets.Num());

		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData || SplineData->SplineStruct.GetNumberOfSplineSegments() <= 0) { continue; }

			const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			if (Config.bUseOctree || Config.bTestInclusionOnProjection)
			{
				SplineData->SplineStruct.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(Config.Fidelity), SplinePoints);

				if (Config.bUseOctree)
				{
					FBox Box = FBox(ForceInit);
					for (int i = 0; i < SplinePoints.Num(); i++) { Box += SplinePoints[i]; }

					double Tol = Config.Tolerance;

					if (Config.bSplineScalesTolerance)
					{
						const FInterpCurveVector& Scales = SplineData->SplineStruct.GetSplinePointsScale();
						for (int i = 0; i < Scales.Points.Num(); i++) { Tol = FMath::Max(Tol, Scales.Points[i].OutVal.Length() * Config.Tolerance); }
					}

					OctreeBounds += BoundsList.Add_GetRef(Box.ExpandBy(Tol));
				}
			}

			if (Config.bTestInclusionOnProjection)
			{
				TArray<FVector2D>& Polygon = Polygons->Emplace_GetRef();

				Polygon.SetNumUninitialized(SplinePoints.Num());
				const FQuat& Projection = Projections->Emplace_GetRef(FQuat::FindBetweenNormals(SplineData->GetTransform().GetRotation().GetUpVector(), FVector::UpVector));

				for (int i = 0; i < SplinePoints.Num(); i++) { Polygon[i] = FVector2D(Projection.RotateVector(SplinePoints[i])); }
				if (!PCGExGeo::IsWinded(EPCGExWinding::CounterClockwise, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(Polygon) < 0)) { Algo::Reverse(Polygon); }
			}

			Splines->Add(SplineData->SplineStruct);
		}
	}

	if (Splines->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); }
		return false;
	}

	if (Config.bUseOctree)
	{
		Octree = MakeShared<PCGEx::FIndexedItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
		for (int i = 0; i < BoundsList.Num(); i++) { Octree->AddElement(PCGEx::FIndexedItem(i, BoundsList[i])); }
	}

	return true;
}

void UPCGExSplineInclusionFilterFactory::BeginDestroy()
{
	Splines.Reset();
	Super::BeginDestroy();
}

namespace PCGExPointFilter
{
	bool FSplineInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		ToleranceSquared = FMath::Square(TypedFilterFactory->Config.Tolerance);

		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

		switch (TypedFilterFactory->Config.CheckType)
		{
		case EPCGExSplineCheckType::IsInside:
			GoodFlags = Inside;
			BadFlags = On;
			GoodMatch = Any;
			bFastInclusionCheck = TypedFilterFactory->Config.bTestInclusionOnProjection && TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::All;
			break;
		case EPCGExSplineCheckType::IsInsideOrOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Inside | On);
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsInsideAndOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Inside | On);
			GoodMatch = All;
			break;
		case EPCGExSplineCheckType::IsOutside:
			GoodFlags = Outside;
			BadFlags = On;
			GoodMatch = Any;
			bFastInclusionCheck = TypedFilterFactory->Config.bTestInclusionOnProjection && TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::All;
			break;
		case EPCGExSplineCheckType::IsOutsideOrOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Outside | On);
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideAndOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Outside | On);
			GoodMatch = All;
			break;
		case EPCGExSplineCheckType::IsOn:
			GoodFlags = On;
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsNotOn:
			BadFlags = On;
			GoodMatch = Skip;
			break;
		}

		return true;
	}

#define PCGEX_CHECK_MAX if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
#define PCGEX_CHECK_MIN if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }

	bool FSplineInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		ESplineCheckFlags State = None;

		int32 InclusionsCount = 0;

		const FVector Pos = Point.Transform.GetLocation();

		if (bFastInclusionCheck)
		{
			if (Octree)
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](const PCGEx::FIndexedItem& Item)
					{
						UpdateInclusionFast(Pos, Item.Index, State, InclusionsCount);
					});

				if (InclusionsCount == 0) { EnumAddFlags(State, Outside); }

				PCGEX_CHECK_MAX
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++)
				{
					UpdateInclusionFast(Pos, i, State, InclusionsCount);
					PCGEX_CHECK_MAX
				}
			}

			PCGEX_CHECK_MIN
		}
		else if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;

			if (Octree)
			{
				int32 Ping = 0;
				
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](
					const PCGEx::FIndexedItem& Item)
					{
						Ping++;
						UpdateInclusionClosest(Pos, Item.Index, State, ClosestDist);
					});

				if (Ping == 0) { EnumAddFlags(State, Outside); }
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++) { UpdateInclusionClosest(Pos, i, State, ClosestDist); }
			}
		}
		else
		{
			if (Octree)
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](
					const PCGEx::FIndexedItem& Item)
					{
						UpdateInclusionFast(Pos, Item.Index, State, InclusionsCount);
					});

				if (InclusionsCount == 0) { EnumAddFlags(State, Outside); }

				PCGEX_CHECK_MAX
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++)
				{
					UpdateInclusionFast(Pos, i, State, InclusionsCount);
					PCGEX_CHECK_MAX
				}
			}

			PCGEX_CHECK_MIN
		}


		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? EnumHasAnyFlags(State, GoodFlags) : EnumHasAllFlags(State, GoodFlags); } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FSplineInclusionFilter::Test(const int32 PointIndex) const
	{
		ESplineCheckFlags State = None;
		int32 InclusionsCount = 0;
		const FVector Pos = InTransforms[PointIndex].GetLocation();

		if (bFastInclusionCheck)
		{
			if (Octree)
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](
					const PCGEx::FIndexedItem& Item)
					{
						UpdateInclusionFast(Pos, Item.Index, State, InclusionsCount);
					});

				if (InclusionsCount == 0) { EnumAddFlags(State, Outside); }

				PCGEX_CHECK_MAX
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++)
				{
					UpdateInclusionFast(Pos, i, State, InclusionsCount);
					PCGEX_CHECK_MAX
				}
			}

			PCGEX_CHECK_MIN
		}
		else if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;

			if (Octree)
			{
				int32 Ping = 0;

				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](
					const PCGEx::FIndexedItem& Item)
					{
						Ping++;
						UpdateInclusionClosest(Pos, Item.Index, State, ClosestDist);
					});

				if (Ping == 0) { EnumAddFlags(State, Outside); }
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++) { UpdateInclusionClosest(Pos, i, State, ClosestDist); }
			}
		}
		else
		{
			if (Octree)
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(Pos, FVector::OneVector), [&](
					const PCGEx::FIndexedItem& Item)
					{
						UpdateInclusion(Pos, Item.Index, State, InclusionsCount);
					});

				if (InclusionsCount == 0) { EnumAddFlags(State, Outside); }

				PCGEX_CHECK_MAX
			}
			else
			{
				for (int i = 0; i < Splines->Num(); i++)
				{
					UpdateInclusion(Pos, i, State, InclusionsCount);
					PCGEX_CHECK_MAX
				}
			}

			PCGEX_CHECK_MIN
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? EnumHasAnyFlags(State, GoodFlags) : EnumHasAllFlags(State, GoodFlags); } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

#undef PCGEX_CHECK_MAX
#undef PCGEX_CHECK_MIN

	void FSplineInclusionFilter::UpdateInclusionFast(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, int32& OutInclusionsCount) const
	{
		if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + TargetIndex)->RotateVector(Pos)), *(Polygons->GetData() + TargetIndex)))
		{
			OutInclusionsCount++;
			EnumAddFlags(OutFlags, Inside);
		}
		else { EnumAddFlags(OutFlags, Outside); }
	}

	void FSplineInclusionFilter::UpdateInclusionClosest(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, double& OutClosestDist) const
	{
		const FPCGSplineStruct& Spline = *(Splines->GetData() + TargetIndex);

		const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
		const FVector& TLoc = T.GetLocation();
		const double D = FVector::DistSquared(Pos, TLoc);

		if (D > OutClosestDist) { return; }
		OutClosestDist = D;

		if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { EnumAddFlags(OutFlags, On); }
		else { EnumRemoveFlags(OutFlags, On); }

		if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
		{
			if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
			{
				EnumAddFlags(OutFlags, Inside);
				EnumRemoveFlags(OutFlags, Outside);
			}
			else
			{
				EnumRemoveFlags(OutFlags, Inside);
				EnumAddFlags(OutFlags, Outside);
			}
		}
		else
		{
			if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + TargetIndex)->RotateVector(Pos)), *(Polygons->GetData() + TargetIndex)))
			{
				EnumRemoveFlags(OutFlags, Inside);
				EnumAddFlags(OutFlags, Inside);
			}
			else
			{
				EnumRemoveFlags(OutFlags, Inside);
				EnumAddFlags(OutFlags, Outside);
			}
		}
	}

	void FSplineInclusionFilter::UpdateInclusion(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, int32& OutInclusionsCount) const
	{
		const FPCGSplineStruct& Spline = *(Splines->GetData() + TargetIndex);

		const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
		const FVector& TLoc = T.GetLocation();
		if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared)
		{
			EnumAddFlags(OutFlags, On);
		}

		if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
		{
			if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
			{
				OutInclusionsCount++;
				EnumAddFlags(OutFlags, Inside);
			}
			else
			{
				EnumAddFlags(OutFlags, Outside);
			}
		}
		else
		{
			if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + TargetIndex)->RotateVector(Pos)), *(Polygons->GetData() + TargetIndex)))
			{
				OutInclusionsCount++;
				EnumAddFlags(OutFlags, Inside);
			}
			else { EnumAddFlags(OutFlags, Outside); }
		}
	}
}

TArray<FPCGPinProperties> UPCGExSplineInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(SplineInclusion)

#if WITH_EDITOR
FString UPCGExSplineInclusionFilterProviderSettings::GetDisplayName() const
{
	switch (Config.CheckType)
	{
	default:
	case EPCGExSplineCheckType::IsInside: return TEXT("Is Inside");
	case EPCGExSplineCheckType::IsInsideOrOn: return TEXT("Is Inside or On");
	case EPCGExSplineCheckType::IsInsideAndOn: return TEXT("Is Outside and On");
	case EPCGExSplineCheckType::IsOutside: return TEXT("Is Outside");
	case EPCGExSplineCheckType::IsOutsideOrOn: return TEXT("Is Outside or On");
	case EPCGExSplineCheckType::IsOutsideAndOn: return TEXT("Is Outside and On");
	case EPCGExSplineCheckType::IsOn: return TEXT("Is On");
	case EPCGExSplineCheckType::IsNotOn: return TEXT("Is not On");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

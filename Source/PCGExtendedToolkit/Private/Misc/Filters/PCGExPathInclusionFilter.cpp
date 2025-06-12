// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathInclusionFilter.h"


#define LOCTEXT_NAMESPACE "PCGExPathInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathInclusionFilterDefinition

bool UPCGExPathInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

bool UPCGExPathInclusionFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExPathInclusionFilterFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	Splines = MakeShared<TArray<TSharedPtr<FPCGSplineStruct>>>();
	TArray<FBox> BoundsList;
	FBox OctreeBounds = FBox(ForceInit);

	if (Config.bTestInclusionOnProjection) { Polygons = MakeShared<TArray<TArray<FVector2D>>>(); }

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::SourcePathsLabel);
		!Targets.IsEmpty())
	{
		BoundsList.Reserve(Targets.Num());

		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data);
			const int32 NumPoints = PathData->GetNumPoints();
			if (!PathData || NumPoints < 2) { continue; }

			const bool bIsClosedLoop = PCGExPaths::GetClosedLoop(PathData);
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			TConstPCGValueRange<FTransform> InTransforms = PathData->GetConstTransformValueRange();

			if (Config.bUseOctree)
			{
				double Tol = Config.Tolerance;

				if (Config.bSplineScalesTolerance)
				{
					for (int i = 0; i < InTransforms.Num(); i++) { Tol = FMath::Max(Tol, InTransforms[i].GetScale3D().Length() * Config.Tolerance); }
				}

				OctreeBounds += BoundsList.Add_GetRef(PathData->GetBounds().ExpandBy(Tol));
			}

			if (Config.bTestInclusionOnProjection)
			{
				//SplineData->SplineStruct.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(Config.Fidelity), SplinePoints);
				TArray<FVector2D>& Polygon = Polygons->Emplace_GetRef();

				Polygon.SetNumUninitialized(NumPoints);
				//const FQuat& Projection = Projections->Emplace_GetRef(FQuat::FindBetweenNormals(SplineData->GetTransform().GetRotation().GetUpVector(), FVector::UpVector));
				const FQuat Projection = FQuat::FindBetweenNormals(FVector::UpVector, FVector::UpVector);

				//Projection.RotateVector(Pos)
				for (int i = 0; i < NumPoints; i++) { Polygon[i] = FVector2D(InTransforms[i].GetLocation()); }
				if (!PCGExGeo::IsWinded(EPCGExWinding::CounterClockwise, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(Polygon) < 0)) { Algo::Reverse(Polygon); }
			}

			TSharedPtr<FPCGSplineStruct> SplineStruct = PCGExPaths::MakeSplineFromPoints(PathData->GetConstTransformValueRange(), Config.PointType, bIsClosedLoop, true);
			Splines->Add(SplineStruct);
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

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPathInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPathInclusionFilter>(this);
}

void UPCGExPathInclusionFilterFactory::BeginDestroy()
{
	Splines.Reset();
	Super::BeginDestroy();
}

namespace PCGExPointFilter
{
	bool FPathInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
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

	bool FPathInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		ESplineCheckFlags State = None;

		const TArray<TSharedPtr<FPCGSplineStruct>>& SplinesRef = *Splines.Get();
		const FVector Pos = Point.Transform.GetLocation();

		int32 InclusionsCount = 0;

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

	bool FPathInclusionFilter::Test(const int32 PointIndex) const
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

	void FPathInclusionFilter::UpdateInclusionFast(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, int32& OutInclusionsCount) const
	{
		if (FGeomTools2D::IsPointInPolygon(FVector2D(Pos), *(Polygons->GetData() + TargetIndex)))
		{
			OutInclusionsCount++;
			EnumAddFlags(OutFlags, Inside);
		}
		else { EnumAddFlags(OutFlags, Outside); }
	}

	void FPathInclusionFilter::UpdateInclusionClosest(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, double& OutClosestDist) const
	{
		const FTransform T = PCGExPaths::GetClosestTransform(*(Splines->GetData() + TargetIndex), Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
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
			if (FGeomTools2D::IsPointInPolygon(FVector2D(Pos), *(Polygons->GetData() + TargetIndex)))
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
	}

	void FPathInclusionFilter::UpdateInclusion(const FVector& Pos, const int32 TargetIndex, ESplineCheckFlags& OutFlags, int32& OutInclusionsCount) const
	{
		const FTransform T = PCGExPaths::GetClosestTransform(*(Splines->GetData() + TargetIndex), Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
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
			if (FGeomTools2D::IsPointInPolygon(FVector2D(Pos), *(Polygons->GetData() + TargetIndex)))
			{
				OutInclusionsCount++;
				EnumAddFlags(OutFlags, Inside);
			}
			else
			{
				EnumAddFlags(OutFlags, Outside);
			}
		}
	}
}

TArray<FPCGPinProperties> UPCGExPathInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(PathInclusion)

#if WITH_EDITOR
FString UPCGExPathInclusionFilterProviderSettings::GetDisplayName() const
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

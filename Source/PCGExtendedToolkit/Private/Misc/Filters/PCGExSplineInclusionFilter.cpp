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
	if (Config.bTestInclusionOnProjection)
	{
		Polygons = MakeShared<TArray<TArray<FVector2D>>>();
		Projections = MakeShared<TArray<FQuat>>();
	}

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
		!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData || SplineData->SplineStruct.GetNumberOfSplineSegments() <= 0) { continue; }

			const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			if (Config.bTestInclusionOnProjection)
			{
				TArray<FVector> SplinePoints;

				SplineData->SplineStruct.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(Config.Fidelity), SplinePoints);
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

	bool FSplineInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		ESplineCheckFlags State = None;

		int32 InclusionsCount = 0;

		const FVector Pos = Point.Transform.GetLocation();

		if (bFastInclusionCheck)
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);
				if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
				{
					InclusionsCount++;
					EnumAddFlags(State, Inside);
				}
				else { EnumAddFlags(State, Outside); }

				if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			}

			if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }
		}
		else if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);

				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				const double D = FVector::DistSquared(Pos, TLoc);

				if (D > ClosestDist) { continue; }
				ClosestDist = D;

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { EnumAddFlags(State, On); }
				else { EnumRemoveFlags(State, On); }

				if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
				{
					if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
					{
						EnumAddFlags(State, Inside);
						EnumRemoveFlags(State, Outside);
					}
					else
					{
						EnumRemoveFlags(State, Inside);
						EnumAddFlags(State, Outside);
					}
				}
				else
				{
					if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
					{
						EnumAddFlags(State, Inside);
						EnumRemoveFlags(State, Outside);
					}
					else
					{
						EnumRemoveFlags(State, Inside);
						EnumAddFlags(State, Outside);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);

				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared)
				{
					EnumAddFlags(State, On);
				}

				if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
				{
					if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
					{
						InclusionsCount++;
						EnumAddFlags(State, Inside);
					}
					else { EnumAddFlags(State, Outside); }
				}
				else
				{
					if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
					{
						InclusionsCount++;
						EnumAddFlags(State, Inside);
					}
					else { EnumAddFlags(State, Outside); }
				}
			}

			if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
		}

		if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }

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
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);
				if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
				{
					InclusionsCount++;
					EnumAddFlags(State, Inside);
				}
				else { EnumAddFlags(State, Outside); }

				if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			}

			if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }
		}
		else if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);

				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				const double D = FVector::DistSquared(Pos, TLoc);

				if (D > ClosestDist) { continue; }
				ClosestDist = D;

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { EnumAddFlags(State, On); }
				else { EnumRemoveFlags(State, On); }

				if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
				{
					if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
					{
						EnumAddFlags(State, Inside);
						EnumRemoveFlags(State, Outside);
					}
					else
					{
						EnumRemoveFlags(State, Inside);
						EnumAddFlags(State, Outside);
					}
				}
				else
				{
					if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
					{
						EnumRemoveFlags(State, Inside);
						EnumAddFlags(State, Inside);
					}
					else
					{
						EnumRemoveFlags(State, Inside);
						EnumAddFlags(State, Outside);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct& Spline = *(Splines->GetData() + i);

				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared)
				{
					EnumAddFlags(State, On);
				}

				if (!TypedFilterFactory->Config.bTestInclusionOnProjection)
				{
					if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
					{
						InclusionsCount++;
						EnumAddFlags(State, Inside);
					}
					else
					{
						EnumAddFlags(State, Outside);
					}
				}
				else
				{
					if (FGeomTools2D::IsPointInPolygon(FVector2D((Projections->GetData() + i)->RotateVector(Pos)), *(Polygons->GetData() + i)))
					{
						InclusionsCount++;
						EnumAddFlags(State, Inside);
					}
					else { EnumAddFlags(State, Outside); }
				}

				if (TypedFilterFactory->Config.bUseMaxInclusionCount && InclusionsCount > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			}

			if (TypedFilterFactory->Config.bUseMinInclusionCount && InclusionsCount < TypedFilterFactory->Config.MinInclusionCount) { return TypedFilterFactory->Config.bInvert; }
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? EnumHasAnyFlags(State, GoodFlags) : EnumHasAllFlags(State, GoodFlags); } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
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

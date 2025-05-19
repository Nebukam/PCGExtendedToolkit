// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeo.h"

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "PCGExFitting.h"
#include "Curve/CurveUtil.h"
#include "Data/PCGExData.h"

bool FPCGExGeo2DProjectionDetails::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& PointDataFacade)
{
	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, FVector::UpVector);
	ProjectionQuat = FQuat::FindBetweenNormals(ProjectionNormal, FVector::UpVector);
	ProjectionInverseQuat = ProjectionInverseQuat.Inverse();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal && PointDataFacade)
	{
		NormalGetter = PointDataFacade->GetBroadcaster<FVector>(LocalNormal);
		if (!NormalGetter)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing normal attribute for projection."));
			return false;
		}
	}

	return true;
}

FQuat FPCGExGeo2DProjectionDetails::GetQuat(const int32 PointIndex) const
{
	return NormalGetter ? FQuat::FindBetweenNormals(NormalGetter->Read(PointIndex).GetSafeNormal(1E-08, FVector::UpVector), FVector::UpVector) :
		       ProjectionQuat;
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition, const int32 PointIndex) const
{
	return NormalGetter ? FQuat::FindBetweenNormals(NormalGetter->Read(PointIndex).GetSafeNormal(1E-08, FVector::UpVector), FVector::UpVector).RotateVector(InPosition) :
		       ProjectionQuat.RotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition) const
{
	return ProjectionQuat.RotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition) const
{
	FVector RotatedPosition = ProjectionQuat.RotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition, const int32 PointIndex) const
{
	FVector RotatedPosition = GetQuat(PointIndex).RotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform) const
{
	FVector Position = ProjectionQuat.RotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(FQuat::FindBetweenNormals(Quat.GetUpVector(), FVector::UpVector) * Quat, Position);
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform, const int32 PointIndex) const
{
	FVector Position = GetQuat(PointIndex).RotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(FQuat::FindBetweenNormals(Quat.GetUpVector(), FVector::UpVector) * Quat, Position);
}

void FPCGExGeo2DProjectionDetails::Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		for (int i = 0; i < NumVectors; i++)
		{
			OutPositions[i] = FQuat::FindBetweenNormals(
				NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
				FVector::UpVector).RotateVector(InPositions[i]);
		}
	}
	else
	{
		for (int i = 0; i < NumVectors; i++)
		{
			OutPositions[i] = ProjectionQuat.RotateVector(InPositions[i]);
		}
	}
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	for (int i = 0; i < NumVectors; i++) { OutPositions[i] = ProjectionQuat.RotateVector(InPositions[i]); }
}

void FPCGExGeo2DProjectionDetails::Project(const TArray<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		for (int i = 0; i < NumVectors; i++)
		{
			OutPositions[i] = FVector2D(
				FQuat::FindBetweenNormals(
					NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
					FVector::UpVector).RotateVector(InPositions[i]));
		}
	}
	else
	{
		for (int i = 0; i < NumVectors; i++)
		{
			OutPositions[i] = FVector2D(ProjectionQuat.RotateVector(InPositions[i]));
		}
	}
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	for (int i = 0; i < NumVectors; i++) { OutPositions[i] = FVector2D(ProjectionQuat.RotateVector(InPositions[i])); }
}

void FPCGExGeo2DProjectionDetails::Project(const TArray<FPCGPoint>& InPoints, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPoints.Num();
	PCGEx::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		for (int i = 0; i < NumVectors; i++)
		{
			OutPositions[i] = FQuat::FindBetweenNormals(
				NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
				FVector::UpVector).RotateVector(InPoints[i].Transform.GetLocation());
		}
	}
	else
	{
		for (int i = 0; i < NumVectors; i++) { OutPositions[i] = ProjectionQuat.RotateVector(InPoints[i].Transform.GetLocation()); }
	}
}

void PCGExGeoTasks::FTransformPointIO::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	UPCGBasePointData* OutPointData = ToBeTransformedIO->GetOut();
	TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();

	FTransform TargetTransform = FTransform::Identity;

	FBox PointBounds = FBox(ForceInit);

	if (!TransformDetails->bIgnoreBounds)
	{
		for (int i = 0; i < OutTransforms.Num(); i++) { PointBounds += OutPointData->GetLocalBounds(i).TransformBy(OutTransforms[i]); }
	}
	else
	{
		for (const FTransform& Pt : OutTransforms) { PointBounds += Pt.GetLocation(); }
	}

	PointBounds = PointBounds.ExpandBy(0.1); // Avoid NaN
	TransformDetails->ComputeTransform(TaskIndex, TargetTransform, PointBounds);

	if (TransformDetails->bInheritRotation && TransformDetails->bInheritScale)
	{
		for (FTransform& Transform : OutTransforms) { Transform *= TargetTransform; }
	}
	else
	{
		if (TransformDetails->bInheritRotation)
		{
			for (FTransform& Transform : OutTransforms)
			{
				FVector OriginalScale = Transform.GetScale3D();
				Transform *= TargetTransform;
				Transform.SetScale3D(OriginalScale);
			}
		}
		else if (TransformDetails->bInheritScale)
		{
			for (FTransform& Transform : OutTransforms)
			{
				FQuat OriginalRot = Transform.GetRotation();
				Transform *= TargetTransform;
				Transform.SetRotation(OriginalRot);
			}
		}
		else
		{
			for (FTransform& Transform : OutTransforms)
			{
				Transform.SetLocation(TargetTransform.TransformPosition(Transform.GetLocation()));
			}
		}
	}
}

namespace PCGExGeo
{
	bool IsWinded(const EPCGExWinding Winding, const bool bIsInputClockwise)
	{
		if (Winding == EPCGExWinding::Clockwise) { return bIsInputClockwise; }
		return !bIsInputClockwise;
	}

	bool IsWinded(const EPCGExWindingMutation Winding, const bool bIsInputClockwise)
	{
		if (Winding == EPCGExWindingMutation::Clockwise) { return bIsInputClockwise; }
		return !bIsInputClockwise;
	}

	FPolygonInfos::FPolygonInfos(const TArray<FVector2D>& InPolygon)
	{
		Area = UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(InPolygon);
		Perimeter = UE::Geometry::CurveUtil::ArcLength<double, FVector2D>(InPolygon, true);

		if (Area < 0)
		{
			bIsClockwise = true;
			Area = FMath::Abs(Area);
		}
		else
		{
			bIsClockwise = false;
		}

		if (Perimeter == 0.0f) { Compactness = 0; }
		else { Compactness = (4.0f * PI * Area) / (Perimeter * Perimeter); }
	}

	bool FPolygonInfos::IsWinded(const EPCGExWinding Winding) const { return PCGExGeo::IsWinded(Winding, bIsClockwise); }

	bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere)
	{
		//Shamelessly stolen from https://stackoverflow.com/questions/37449046/how-to-calculate-the-sphere-center-with-4-points

		const double U = S_U(A, B, C, D, B, C, D, A);
		const double V = S_U(C, D, A, B, D, A, B, C);
		const double W = S_U(A, C, D, B, B, D, A, C);
		const double UVW = 2 * (U + V + W);

		if (UVW == 0.0) { return false; } // Coplanar

		constexpr int C_X = 0;
		constexpr int C_Y = 1;
		constexpr int C_Z = 2;
		const double RA = S_SQ(A);
		const double RB = S_SQ(B);
		const double RC = S_SQ(C);
		const double RD = S_SQ(D);

		const FVector Center = FVector(
			S_E(C_Y, C_Z, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_Z, C_X, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_X, C_Y, A, B, C, D, RA, RB, RC, RD, UVW));

		const double radius = FMath::Sqrt(S_SQ(FVector(A - Center)));

		OutSphere = FSphere(Center, radius);
		return true;
	}

	bool FindSphereFrom4Points(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FSphere& OutSphere)
	{
		return FindSphereFrom4Points(
			Positions[Vtx[0]],
			Positions[Vtx[1]],
			Positions[Vtx[2]],
			Positions[Vtx[3]],
			OutSphere);
	}

	void GetCircumcenter(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter)
	{
		// Calculate midpoints of two sides
		const FVector& A = Positions[Vtx[0]];
		const FVector& B = Positions[Vtx[1]];
		const FVector& C = Positions[Vtx[2]];


		const FVector AC = C - A;
		const FVector AB = B - A;
		const FVector ABxAC = AB.Cross(AC);

		// this is the vector from a TO the circumsphere center
		const FVector ToCircumsphereCenter = (ABxAC.Cross(AB) * AC.SquaredLength() + AC.Cross(ABxAC) * AB.SquaredLength()) / (2 * ABxAC.SquaredLength());
		//float Radius = ToCircumsphereCenter.Length();

		// The 3 space coords of the circumsphere center then:
		OutCircumcenter = A + ToCircumsphereCenter;
	}

	void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FVector& OutCentroid)
	{
		OutCentroid = FVector::Zero();
		for (int i = 0; i < 4; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 4;
	}

	void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCentroid)
	{
		OutCentroid = FVector::Zero();
		for (int i = 0; i < 3; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 3;
	}

	void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], uint64& Edge)
	{
		double Dist = 0;
		for (int i = 0; i < 3; i++)
		{
			for (int j = i + 1; j < 3; j++)
			{
				const double LocalDist = FVector::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64U(Vtx[i], Vtx[j]);
				}
			}
		}
	}

	void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], uint64& Edge)
	{
		double Dist = 0;
		for (int i = 0; i < 4; i++)
		{
			for (int j = i + 1; j < 4; j++)
			{
				const double LocalDist = FVector::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64U(Vtx[i], Vtx[j]);
				}
			}
		}
	}

	void PointsToPositions(const TArray<FPCGPoint>& Points, TArray<FVector>& OutPositions)
	{
		const int32 NumPoints = Points.Num();
		PCGEx::InitArray(OutPositions, NumPoints);
		for (int i = 0; i < NumPoints; i++) { OutPositions[i] = Points[i].Transform.GetLocation(); }
	}

	FVector GetBarycentricCoordinates(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector AB = B - A;
		const FVector AC = C - A;
		const FVector AD = Point - A;

		const double d00 = FVector::DotProduct(AB, AB);
		const double d01 = FVector::DotProduct(AB, AC);
		const double d11 = FVector::DotProduct(AC, AC);
		const double d20 = FVector::DotProduct(AD, AB);
		const double d21 = FVector::DotProduct(AD, AC);

		const double Den = d00 * d11 - d01 * d01;
		const double V = (d11 * d20 - d01 * d21) / Den;
		const double W = (d00 * d21 - d01 * d20) / Den;
		const double U = 1.0f - V - W;

		return FVector(U, V, W);
	}

	bool IsPointInTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector& D = FVector::CrossProduct(B - A, P - A);
		return (FVector::DotProduct(D, FVector::CrossProduct(C - B, P - B)) >= 0) &&
			(FVector::DotProduct(D, FVector::CrossProduct(A - C, P - C)) >= 0);
	}

	FApex::FApex(const FVector& Start, const FVector& End, const FVector& InApex)
	{
		Direction = (Start - End).GetSafeNormal();
		Anchor = FMath::ClosestPointOnSegment(InApex, Start, End);

		const double DistToStart = FVector::Dist(Start, Anchor);
		const double DistToEnd = FVector::Dist(End, Anchor);
		TowardStart = Direction * (DistToStart * -1);
		TowardEnd = Direction * DistToEnd;
		Alpha = DistToStart / (DistToStart + DistToEnd);
	}

	void FApex::Scale(const double InScale)
	{
		TowardStart *= InScale;
		TowardEnd *= InScale;
	}

	void FApex::Extend(const double InSize)
	{
		TowardStart += Direction * InSize;
		TowardEnd += Direction * -InSize;
	}

	FExCenterArc::FExCenterArc(const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector Up = PCGExMath::GetNormal(A, B, C);
		bool bIntersect = true;

		Center = PCGExMath::SafeLinePlaneIntersection(
			C, C + PCGExMath::GetNormal(B, C, C + Up),
			A, (A - B).GetSafeNormal(), bIntersect);

		if (!bIntersect) { Center = FMath::Lerp(A, C, 0.5); } // Parallel lines, place center right in the middle

		Radius = FVector::Dist(C, Center);

		Hand = (A - Center).GetSafeNormal();
		OtherHand = (C - Center).GetSafeNormal();

		bIsLine = FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Hand, OtherHand)), 1);

		Normal = FVector::CrossProduct(Hand, OtherHand).GetSafeNormal();
		Theta = FMath::Acos(FVector::DotProduct(Hand, OtherHand));
		SinTheta = FMath::Sin(Theta);
	}

	FExCenterArc::FExCenterArc(const FVector& A1, const FVector& B1, const FVector& A2, const FVector& B2, const double MaxLength)
	{
		const FVector& N1 = PCGExMath::GetNormal(B1, A1, A1 + PCGExMath::GetNormal(B1, A1, A2));
		const FVector& N2 = PCGExMath::GetNormal(B2, A2, A2 + PCGExMath::GetNormal(B2, A2, A1));

		if (FMath::IsNearlyZero(FVector::DotProduct(N1, N2)))
		{
			Center = FMath::Lerp(B1, B2, 0.5);
		}
		else
		{
			FVector OutA = FVector::ZeroVector;
			FVector OutB = FVector::ZeroVector;
			FMath::SegmentDistToSegment(
				B1 + N1 * -MaxLength, B1 + N1 * MaxLength,
				B2 + N2 * -MaxLength, B2 + N2 * MaxLength,
				OutA, OutB);
			Center = FMath::Lerp(OutA, OutB, 0.5);
		}

		Radius = FVector::Dist(A2, Center);

		Hand = (B1 - Center).GetSafeNormal();
		OtherHand = (B2 - Center).GetSafeNormal();

		Normal = FVector::CrossProduct(Hand, OtherHand).GetSafeNormal();
		Theta = FMath::Acos(FVector::DotProduct(Hand, OtherHand));
		SinTheta = FMath::Sin(Theta);
	}

	FVector FExCenterArc::GetLocationOnArc(const double Alpha) const
	{
		const double W1 = FMath::Sin((1.0 - Alpha) * Theta) / SinTheta;
		const double W2 = FMath::Sin(Alpha * Theta) / SinTheta;

		const FVector Dir = Hand * W1 + OtherHand * W2;
		return Center + (Dir * Radius);
	}
}

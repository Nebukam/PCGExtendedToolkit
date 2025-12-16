// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeo.h"

#include "CoreMinimal.h"
#include "MinVolumeBox3.h"
#include "PCGEx.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "Curve/CurveUtil.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"
#include "Async/ParallelFor.h"

namespace PCGExGeo
{
	bool IntersectOBB_OBB(const FBox& BoxA, const FTransform& TransformA, const FBox& BoxB, const FTransform& TransformB)
	{
		// Extents with scale applied
		const FVector ExtentA = BoxA.GetExtent() * TransformA.GetScale3D();
		const FVector ExtentB = BoxB.GetExtent() * TransformB.GetScale3D();

		const double EA0 = ExtentA.X, EA1 = ExtentA.Y, EA2 = ExtentA.Z;
		const double EB0 = ExtentB.X, EB1 = ExtentB.Y, EB2 = ExtentB.Z;

		// Get rotation matrices directly (no scale)
		const FMatrix MatA = TransformA.ToMatrixNoScale();
		const FMatrix MatB = TransformB.ToMatrixNoScale();

		// World-space axes
		const FVector AX = MatA.GetUnitAxis(EAxis::X);
		const FVector AY = MatA.GetUnitAxis(EAxis::Y);
		const FVector AZ = MatA.GetUnitAxis(EAxis::Z);
		const FVector BX = MatB.GetUnitAxis(EAxis::X);
		const FVector BY = MatB.GetUnitAxis(EAxis::Y);
		const FVector BZ = MatB.GetUnitAxis(EAxis::Z);

		// Translation between origins
		const FVector D = MatB.GetOrigin() - MatA.GetOrigin();

		// Translation in A's local frame (dot products)
		const double T0 = AX.X * D.X + AX.Y * D.Y + AX.Z * D.Z;
		const double T1 = AY.X * D.X + AY.Y * D.Y + AY.Z * D.Z;
		const double T2 = AZ.X * D.X + AZ.Y * D.Y + AZ.Z * D.Z;

		// Rotation matrix R[i][j] = dot(A_i, B_j)
		constexpr double Eps = UE_SMALL_NUMBER;

		const double R00 = AX.X * BX.X + AX.Y * BX.Y + AX.Z * BX.Z;
		const double R01 = AX.X * BY.X + AX.Y * BY.Y + AX.Z * BY.Z;
		const double R02 = AX.X * BZ.X + AX.Y * BZ.Y + AX.Z * BZ.Z;
		const double R10 = AY.X * BX.X + AY.Y * BX.Y + AY.Z * BX.Z;
		const double R11 = AY.X * BY.X + AY.Y * BY.Y + AY.Z * BY.Z;
		const double R12 = AY.X * BZ.X + AY.Y * BZ.Y + AY.Z * BZ.Z;
		const double R20 = AZ.X * BX.X + AZ.Y * BX.Y + AZ.Z * BX.Z;
		const double R21 = AZ.X * BY.X + AZ.Y * BY.Y + AZ.Z * BY.Z;
		const double R22 = AZ.X * BZ.X + AZ.Y * BZ.Y + AZ.Z * BZ.Z;

		const double AR00 = FMath::Abs(R00) + Eps, AR01 = FMath::Abs(R01) + Eps, AR02 = FMath::Abs(R02) + Eps;
		const double AR10 = FMath::Abs(R10) + Eps, AR11 = FMath::Abs(R11) + Eps, AR12 = FMath::Abs(R12) + Eps;
		const double AR20 = FMath::Abs(R20) + Eps, AR21 = FMath::Abs(R21) + Eps, AR22 = FMath::Abs(R22) + Eps;

		// A's local axes (3 tests)
		if (FMath::Abs(T0) > EA0 + EB0 * AR00 + EB1 * AR01 + EB2 * AR02){return false;}
		if (FMath::Abs(T1) > EA1 + EB0 * AR10 + EB1 * AR11 + EB2 * AR12){return false;}
		if (FMath::Abs(T2) > EA2 + EB0 * AR20 + EB1 * AR21 + EB2 * AR22){return false;}

		// B's local axes (3 tests)
		if (FMath::Abs(T0 * R00 + T1 * R10 + T2 * R20) > EA0 * AR00 + EA1 * AR10 + EA2 * AR20 + EB0){ return false;}
		if (FMath::Abs(T0 * R01 + T1 * R11 + T2 * R21) > EA0 * AR01 + EA1 * AR11 + EA2 * AR21 + EB1){ return false;}
		if (FMath::Abs(T0 * R02 + T1 * R12 + T2 * R22) > EA0 * AR02 + EA1 * AR12 + EA2 * AR22 + EB2){ return false;}

		// Cross product axes (9 tests)
		if (FMath::Abs(T2 * R10 - T1 * R20) > EA1 * AR20 + EA2 * AR10 + EB1 * AR02 + EB2 * AR01){ return false;}
		if (FMath::Abs(T2 * R11 - T1 * R21) > EA1 * AR21 + EA2 * AR11 + EB0 * AR02 + EB2 * AR00){ return false;}
		if (FMath::Abs(T2 * R12 - T1 * R22) > EA1 * AR22 + EA2 * AR12 + EB0 * AR01 + EB1 * AR00){ return false;}
		if (FMath::Abs(T0 * R20 - T2 * R00) > EA0 * AR20 + EA2 * AR00 + EB1 * AR12 + EB2 * AR11){ return false;}
		if (FMath::Abs(T0 * R21 - T2 * R01) > EA0 * AR21 + EA2 * AR01 + EB0 * AR12 + EB2 * AR10){ return false;}
		if (FMath::Abs(T0 * R22 - T2 * R02) > EA0 * AR22 + EA2 * AR02 + EB0 * AR11 + EB1 * AR10){ return false;}
		if (FMath::Abs(T1 * R00 - T0 * R10) > EA0 * AR10 + EA1 * AR00 + EB1 * AR22 + EB2 * AR21){ return false;}
		if (FMath::Abs(T1 * R01 - T0 * R11) > EA0 * AR11 + EA1 * AR01 + EB0 * AR22 + EB2 * AR20){ return false;}
		if (FMath::Abs(T1 * R02 - T0 * R12) > EA0 * AR12 + EA1 * AR02 + EB0 * AR21 + EB1 * AR20){ return false;}

		return true;
	}

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
		return FindSphereFrom4Points(Positions[Vtx[0]], Positions[Vtx[1]], Positions[Vtx[2]], Positions[Vtx[3]], OutSphere);
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
		OutCentroid = FVector::ZeroVector;
		for (int i = 0; i < 4; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 4;
	}

	void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCentroid)
	{
		OutCentroid = FVector::ZeroVector;
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

	void PointsToPositions(const UPCGBasePointData* InPointData, TArray<FVector>& OutPositions)
	{
		const int32 NumPoints = InPointData->GetNumPoints();
		const TConstPCGValueRange<FTransform> Transforms = InPointData->GetConstTransformValueRange();
		PCGEx::InitArray(OutPositions, NumPoints);
		PCGEX_PARALLEL_FOR(NumPoints, OutPositions[i] = Transforms[i].GetLocation();)
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
		return (FVector::DotProduct(D, FVector::CrossProduct(C - B, P - B)) >= 0) && (FVector::DotProduct(D, FVector::CrossProduct(A - C, P - C)) >= 0);
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

		Center = PCGExMath::SafeLinePlaneIntersection(C, C + PCGExMath::GetNormal(B, C, C + Up), A, (A - B).GetSafeNormal(), bIntersect);

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
			FMath::SegmentDistToSegment(B1 + N1 * -MaxLength, B1 + N1 * MaxLength, B2 + N2 * -MaxLength, B2 + N2 * MaxLength, OutA, OutB);
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

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(InTransforms.Num(), [&](int32 i)
		{
			const FVector P = InTransforms[i].GetLocation();
			Centroid += P;
			return P;
		});

		Centroid /= InTransforms.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms, const TArrayView<int32> InIndices)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(InIndices.Num(), [&](int32 i)
		{
			const FVector P = InTransforms[InIndices[i]].GetLocation();
			Centroid += P;
			return P;
		});

		Centroid /= InTransforms.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector> InPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(InPositions.Num(), [&](int32 i)
		{
			const FVector P = InPositions[i];
			Centroid += P;
			return P;
		});

		Centroid /= InPositions.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector2D> InPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(InPositions.Num(), [&](int32 i)
		{
			const FVector P = FVector(InPositions[i], 0);
			Centroid += P;
			return P;
		});

		Centroid /= InPositions.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(NumElements, [&](int32 i)
		{
			const FVector P = GetPointFunc(i);
			Centroid += P;
			return P;
		});

		Centroid /= NumElements;

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc, const FVector& Extra)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(NumElements + 1, [&](int32 i)
		{
			const FVector P = i == NumElements ? Extra : GetPointFunc(i);
			Centroid += P;
			return P;
		});

		Centroid /= NumElements;

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FTransform FBestFitPlane::GetTransform() const
	{
		FTransform Transform = FTransform(FMatrix(Axis[0], Axis[1], Axis[2], FVector::Zero()));
		Transform.SetLocation(Centroid);
		Transform.SetScale3D(FVector::OneVector);
		return Transform;
	}

	FTransform FBestFitPlane::GetTransform(const EPCGExAxisOrder Order) const
	{
		int32 Comps[3] = {0, 0, 0};
		PCGEx::GetAxesOrder(Order, Comps);

		FTransform Transform = FTransform(FMatrix(Axis[Comps[0]], Axis[Comps[1]], Axis[Comps[2]], FVector::Zero()));
		Transform.SetLocation(Centroid);
		Transform.SetScale3D(FVector::OneVector);
		return Transform;
	}

	void FBestFitPlane::ProcessBox(const UE::Geometry::FOrientedBox3d& Box)
	{
		Centroid = Box.Center();

		Algo::Sort(Swizzle, [&](const int32 A, const int32 B) { return Box.Extents[A] > Box.Extents[B]; });

		Extents[0] = Box.Extents[Swizzle[0]];
		Extents[1] = Box.Extents[Swizzle[1]];
		Extents[2] = Box.Extents[Swizzle[2]];

		// Pick raw axes
		FVector X = Box.Frame.GetAxis(Swizzle[0]); // Longest
		FVector Y = Box.Frame.GetAxis(Swizzle[1]); // Median
		FVector Z = Box.Frame.GetAxis(Swizzle[2]); // Smallest

		// Re-orthogonalize using cross product to avoid flip
		// Ensure right-handed system
		Z = FVector::CrossProduct(X, Y).GetSafeNormal();
		Y = FVector::CrossProduct(Z, X).GetSafeNormal();

		// Make sure Z points upward
		if (FVector::DotProduct(Z, FVector::UpVector) < 0) { Z *= -1; }

		Axis[0] = X.GetSafeNormal();
		Axis[1] = Y.GetSafeNormal();
		Axis[2] = Z.GetSafeNormal();
	}
}

FPCGExGeo2DProjectionDetails::FPCGExGeo2DProjectionDetails()
{
	WorldUp = GetDefault<UPCGExGlobalSettings>()->WorldUp;
	WorldFwd = GetDefault<UPCGExGlobalSettings>()->WorldForward;
	ProjectionNormal = WorldUp;
}

FPCGExGeo2DProjectionDetails::FPCGExGeo2DProjectionDetails(const bool InSupportLocalNormal)
	: bSupportLocalNormal(InSupportLocalNormal)
{
	WorldUp = GetDefault<UPCGExGlobalSettings>()->WorldUp;
	ProjectionNormal = WorldUp;
}

bool FPCGExGeo2DProjectionDetails::Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade)
{
	FPCGExContext* Context = PointDataFacade->GetContext();
	if (!Context) { return false; }

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal && PointDataFacade)
	{
		NormalGetter = PCGExDetails::MakeSettingValue<FVector>(EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		if (!NormalGetter->Init(PointDataFacade, false, false))
		{
			NormalGetter = nullptr;
			return false;
		}
	}

	return true;
}

bool FPCGExGeo2DProjectionDetails::Init(const TSharedPtr<PCGExData::FPointIO>& PointIO)
{
	FPCGExContext* Context = PointIO->GetContext();
	if (!Context) { return false; }

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(LocalNormal))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Only @Data domain attributes are supported for local projection."));
		}
		else
		{
			NormalGetter = PCGExDetails::MakeSettingValue<FVector>(PointIO, EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		}

		if (!NormalGetter) { return false; }
	}

	return true;
}

bool FPCGExGeo2DProjectionDetails::Init(const UPCGData* InData)
{
	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(LocalNormal))
		{
			UE_LOG(LogTemp, Warning, TEXT("Only @Data domain attributes are supported for local projection."));
		}
		else
		{
			NormalGetter = PCGExDetails::MakeSettingValue<FVector>(nullptr, InData, EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		}

		if (!NormalGetter) { return false; }
	}

	return true;
}

void FPCGExGeo2DProjectionDetails::Init(const PCGExGeo::FBestFitPlane& InFitPlane)
{
	ProjectionNormal = InFitPlane.Normal();
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();
}

#define PCGEX_READ_QUAT(_INDEX) FRotationMatrix::MakeFromZX(NormalGetter->Read(_INDEX).GetSafeNormal(1E-08, FVector::UpVector), WorldFwd).ToQuat()

FQuat FPCGExGeo2DProjectionDetails::GetQuat(const int32 PointIndex) const
{
	return NormalGetter ? PCGEX_READ_QUAT(PointIndex) : ProjectionQuat;
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition, const int32 PointIndex) const
{
	return GetQuat(PointIndex).UnrotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition) const
{
	return ProjectionQuat.UnrotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition) const
{
	FVector RotatedPosition = ProjectionQuat.UnrotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition, const int32 PointIndex) const
{
	//
	FVector RotatedPosition = GetQuat(PointIndex).UnrotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform) const
{
	FVector Position = ProjectionQuat.UnrotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(Quat * ProjectionQuat, Position);
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat Q = GetQuat(PointIndex);
	FVector Position = Q.UnrotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(Quat * Q, Position);
}

template <typename T>
void FPCGExGeo2DProjectionDetails::ProjectFlat(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<T>& OutPositions) const
{
	const TConstPCGValueRange<FTransform> Transforms = InFacade->Source->GetInOut()->GetConstTransformValueRange();
	const int32 NumVectors = Transforms.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = T(ProjectFlat(Transforms[i].GetLocation(), i));
	)
}

template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector2D>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector2D>& OutPositions) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector>& OutPositions) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector4>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector4>& OutPositions) const;

void FPCGExGeo2DProjectionDetails::Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		PCGEX_PARALLEL_FOR(
			NumVectors,
			OutPositions[i] = PCGEX_READ_QUAT(i).UnrotateVector(InPositions[i]);
		)
	}
	else
	{
		PCGEX_PARALLEL_FOR(
			NumVectors,
			OutPositions[i] = ProjectionQuat.UnrotateVector(InPositions[i]);
		)
	}
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = FVector2D(ProjectionQuat.UnrotateVector(InPositions[i]));
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TConstPCGValueRange<FTransform>& InTransforms, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InTransforms.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = FVector2D(ProjectionQuat.UnrotateVector(InTransforms[i].GetLocation()));
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, std::vector<double>& OutPositions) const
{
	PCGEX_PARALLEL_FOR(
		InPositions.Num(),
		const FVector PP = ProjectionQuat.UnrotateVector(InPositions[i]);
		const int32 ii = i * 2;
		OutPositions[ii] = PP.X;
		OutPositions[ii+1] = PP.Y;
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TConstPCGValueRange<FTransform>& InTransforms, std::vector<double>& OutPositions) const
{
	PCGEX_PARALLEL_FOR(
		InTransforms.Num(),
		const FVector PP = ProjectionQuat.UnrotateVector(InTransforms[i].GetLocation());
		const int32 ii = i * 2;
		OutPositions[ii] = PP.X;
		OutPositions[ii+1] = PP.Y;
	)
}

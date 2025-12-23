// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/Geo/PCGExGeo.h"

#include "CoreMinimal.h"
#include "GeomTools.h"
#include "Data/PCGExPointIO.h"
#include "Async/ParallelFor.h"
#include "Math/PCGExMath.h"
#include "Math/PCGExMathAxis.h"

namespace PCGExMath::Geo
{
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
		const FVector Up = GetNormal(A, B, C);
		bool bIntersect = true;

		Center = SafeLinePlaneIntersection(C, C + GetNormal(B, C, C + Up), A, (A - B).GetSafeNormal(), bIntersect);

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
		const FVector& N1 = GetNormal(B1, A1, A1 + GetNormal(B1, A1, A2));
		const FVector& N2 = GetNormal(B2, A2, A2 + GetNormal(B2, A2, A1));

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


	bool IsPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		return FGeomTools2D::IsPointInPolygon(FVector2D(Point[0], Point[1]), Polygon);
	}

	bool IsPointInPolygon(const FVector& Point, const TArray<FVector2D>& Polygon)
	{
		return FGeomTools2D::IsPointInPolygon(FVector2D(Point[0], Point[1]), Polygon);
	}

	bool IsAnyPointInPolygon(const TArray<FVector2D>& Points, const TArray<FVector2D>& Polygon)
	{
		if (Points.IsEmpty()) { return false; }
		for (const FVector2D& P : Points) { if (FGeomTools2D::IsPointInPolygon(P, Polygon)) { return true; } }
		return false;
	}
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"
#include "PCGExGeo.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGeo2DProjectionSettings
{
	GENERATED_BODY()

	/** Normal vector of the 2D projection plane. Defaults to Up for XY projection. Used as fallback when using invalid local normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable))
	FVector ProjectionNormal = FVector::UpVector;

	/** Uses a per-point projection normal. Use carefully as it can easily lead to singularities. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalProjectionNormal = false;

	/** Fetch the normal from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, ShowOnlyInnerProperties, EditCondition="bUseLocalProjectionNormal"))
	FPCGExInputDescriptor LocalProjectionNormal;
};

UENUM(BlueprintType)
enum class EPCGExCellCenter : uint8
{
	Balanced UMETA(DisplayName = "Balanced", ToolTip="Pick centroid if circumcenter is out of bounds, otherwise uses circumcenter."),
	Circumcenter UMETA(DisplayName = "Canon (Circumcenter)", ToolTip="Uses Delaunay cells' circumcenter."),
	Centroid UMETA(DisplayName = "Centroid", ToolTip="Uses Delaunay cells' averaged vertice positions.")
};

namespace PCGExGeo
{
	constexpr PCGExMT::AsyncState State_ProcessingHull = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunayHull = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunayPreprocess = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunay = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingVoronoi = __COUNTER__;

	static double S_U(
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FVector& E, const FVector& F, const FVector& G, const FVector& H)
	{
		return (A.Z - B.Z) * (C.X * D.Y - D.X * C.Y) - (E.Z - F.Z) * (G.X * H.Y - H.X * G.Y);
	};

	static double S_D(
		const int FirstComponent, const int SecondComponent,
		FVector A, FVector B, FVector C)
	{
		return
			A[FirstComponent] * (B[SecondComponent] - C[SecondComponent]) +
			B[FirstComponent] * (C[SecondComponent] - A[SecondComponent]) +
			C[FirstComponent] * (A[SecondComponent] - B[SecondComponent]);
	};

	static double S_E(
		const int FirstComponent, const int SecondComponent,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const double RA, const double RB, const double RC, const double RD, const double UVW)
	{
		return (RA * S_D(FirstComponent, SecondComponent, B, C, D) - RB * S_D(FirstComponent, SecondComponent, C, D, A) +
			RC * S_D(FirstComponent, SecondComponent, D, A, B) - RD * S_D(FirstComponent, SecondComponent, A, B, C)) / UVW;
	};

	static double S_SQ(const FVector& P) { return P.X * P.X + P.Y * P.Y + P.Z * P.Z; };

	static bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere)
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

	static bool FindSphereFrom4Points(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FSphere& OutSphere)
	{
		return FindSphereFrom4Points(
			Positions[Vtx[0]],
			Positions[Vtx[1]],
			Positions[Vtx[2]],
			Positions[Vtx[3]],
			OutSphere);
	}

	static void GetCircumcenter(const TArrayView<FVector2D>& Positions, const int32 (&Vtx)[3], FVector2D& OutCircumcenter)
	{
		const FVector2D& A = Positions[Vtx[0]];
		const FVector2D& B = Positions[Vtx[1]];
		const FVector2D& C = Positions[Vtx[2]];

		const FVector2D MidAB = (A + B) * 0.5;
		const FVector2D MidBC = (B + C) * 0.5;

		// Slopes of perpendicular bisectors of AB and BC
		const double SlopeAB = -(B.Y - A.Y) / (B.X - A.X);
		const double SlopeBC = -(C.Y - B.Y) / (C.X - B.X);

		// Offsets of perpendicular bisectors
		const double OffsetAB = MidAB.Y - SlopeAB * MidAB.X;
		const double OffsetBC = MidBC.Y - SlopeBC * MidBC.X;

		const double CX = (OffsetBC - OffsetAB) / (SlopeAB - SlopeBC);

		OutCircumcenter.X = (OffsetBC - OffsetAB) / (SlopeAB - SlopeBC);
		OutCircumcenter.Y = SlopeAB * CX + OffsetAB;
	}

	static void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FVector& OutCentroid)
	{
		OutCentroid = FVector::Zero();
		OutCentroid += Positions[Vtx[0]];
		OutCentroid += Positions[Vtx[1]];
		OutCentroid += Positions[Vtx[2]];
		OutCentroid += Positions[Vtx[3]];
		OutCentroid /= 4;
	}

	static void GetCentroid(const TArrayView<FVector2D>& Positions, const int32 (&Vtx)[3], FVector2D& OutCentroid)
	{
		OutCentroid = FVector2D::Zero();
		OutCentroid += Positions[Vtx[0]];
		OutCentroid += Positions[Vtx[1]];
		OutCentroid += Positions[Vtx[2]];
		OutCentroid /= 3;
	}

	static void GetLongestEdge(const TArrayView<FVector2D>& Positions, const int32 (&Vtx)[3], uint64& Edge)
	{
		double Dist = TNumericLimits<double>::Min();
		for (int i = 0; i < 3; i++)
		{
			for (int j = i + 1; j < 3; j++)
			{
				const double LocalDist = FVector2D::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64(Vtx[i], Vtx[j]);
				}
			}
		}
	}

	static void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], uint64& Edge)
	{
		double Dist = TNumericLimits<double>::Min();
		for (int i = 0; i < 4; i++)
		{
			for (int j = i + 1; j < 4; j++)
			{
				const double LocalDist = FVector::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64(Vtx[i], Vtx[j]);
				}
			}
		}
	}
}

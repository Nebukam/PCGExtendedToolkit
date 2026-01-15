// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

#include "PCGExGeo.generated.h"

UENUM()
enum class EPCGExCellCenter : uint8
{
	Balanced     = 0 UMETA(DisplayName = "Balanced", ToolTip="Pick centroid if circumcenter is out of bounds, otherwise uses circumcenter."),
	Circumcenter = 1 UMETA(DisplayName = "Canon (Circumcenter)", ToolTip="Uses Delaunay cells' circumcenter."),
	Centroid     = 2 UMETA(DisplayName = "Centroid", ToolTip="Uses Delaunay cells' averaged vertice positions.")
};

UENUM()
enum class EPCGExVoronoiMetric : uint8
{
	Euclidean = 0 UMETA(DisplayName = "Euclidean (L2)", ToolTip="Standard Euclidean distance. Produces classic Voronoi with straight edges."),
	Manhattan = 1 UMETA(DisplayName = "Manhattan (L1)", ToolTip="Taxicab/Manhattan distance. Produces diamond-shaped cells with axis-aligned and 45-degree edges."),
	Chebyshev = 2 UMETA(DisplayName = "Chebyshev (L-Inf)", ToolTip="Chessboard/Chebyshev distance. Produces square-ish cells with axis-aligned and 45-degree edges.")
};

namespace PCGExMath::Geo
{
	namespace States
	{
		PCGEX_CTX_STATE(State_ExtractingMesh)
	}

	template <typename T>
	FORCEINLINE double Det(const T& A, const T& B) { return A.X * B.Y - A.Y * B.X; }

	FORCEINLINE static double S_U(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& E, const FVector& F, const FVector& G, const FVector& H)
	{
		return (A.Z - B.Z) * (C.X * D.Y - D.X * C.Y) - (E.Z - F.Z) * (G.X * H.Y - H.X * G.Y);
	};

	FORCEINLINE static double S_D(const int FirstComponent, const int SecondComponent, FVector A, FVector B, FVector C)
	{
		return A[FirstComponent] * (B[SecondComponent] - C[SecondComponent]) + B[FirstComponent] * (C[SecondComponent] - A[SecondComponent]) + C[FirstComponent] * (A[SecondComponent] - B[SecondComponent]);
	};

	FORCEINLINE static double S_E(const int FirstComponent, const int SecondComponent, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const double RA, const double RB, const double RC, const double RD, const double UVW)
	{
		return (RA * S_D(FirstComponent, SecondComponent, B, C, D) - RB * S_D(FirstComponent, SecondComponent, C, D, A) + RC * S_D(FirstComponent, SecondComponent, D, A, B) - RD * S_D(FirstComponent, SecondComponent, A, B, C)) / UVW;
	};

	static double S_SQ(const FVector& P) { return P.X * P.X + P.Y * P.Y + P.Z * P.Z; };


	PCGEXCORE_API bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere);
	PCGEXCORE_API bool FindSphereFrom4Points(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FSphere& OutSphere);
	PCGEXCORE_API void GetCircumcenter(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter);

	/** Compute 2D circumcenter (using only X,Y) with Z averaged from input vertices */
	PCGEXCORE_API void GetCircumcenter2D(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter);

	PCGEXCORE_API void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FVector& OutCentroid);
	PCGEXCORE_API void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCentroid);
	PCGEXCORE_API void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], uint64& Edge);
	PCGEXCORE_API void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], uint64& Edge);

	PCGEXCORE_API FVector GetBarycentricCoordinates(const FVector& Point, const FVector& A, const FVector& B, const FVector& C);
	PCGEXCORE_API bool IsPointInTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C);

	/**
		 *	 Leave <---.Apex-----> Arrive (Direction)
		 *		   . '   |    '  .  
		 *		A----Anchor---------B
		 */
	struct PCGEXCORE_API FApex
	{
		FApex()
		{
		}

		FApex(const FVector& Start, const FVector& End, const FVector& InApex);

		FVector Direction = FVector::ZeroVector;
		FVector Anchor = FVector::ZeroVector;
		FVector TowardStart = FVector::ZeroVector;
		FVector TowardEnd = FVector::ZeroVector;
		double Alpha = 0;

		FVector GetAnchorNormal(const FVector& Location) const { return (Anchor - Location).GetSafeNormal(); }

		void Scale(const double InScale);
		void Extend(const double InSize);

		static FApex FromStartOnly(const FVector& Start, const FVector& InApex) { return FApex(Start, InApex, InApex); }
		static FApex FromEndOnly(const FVector& End, const FVector& InApex) { return FApex(InApex, End, InApex); }
	};

	struct PCGEXCORE_API FExCenterArc
	{
		double Radius = 0;
		double Theta = 0;
		double SinTheta = 0;

		FVector Center = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		FVector Hand = FVector::ZeroVector;
		FVector OtherHand = FVector::ZeroVector;

		bool bIsLine = false;

		FExCenterArc()
		{
		}


		/**
			 * ExCenter arc from 3 points.
			 * The arc center will be opposite to B
			 * @param A 
			 * @param B 
			 * @param C 
			 */
		FExCenterArc(const FVector& A, const FVector& B, const FVector& C);

		/**
			 * ExCenter arc from 2 segments.
			 * The arc center will be opposite to B
			 * @param A1 
			 * @param B1 
			 * @param A2 
			 * @param B2
			 * @param MaxLength 
			 */
		FExCenterArc(const FVector& A1, const FVector& B1, const FVector& A2, const FVector& B2, const double MaxLength = 100000);

		FORCEINLINE double GetLength() const { return Radius * Theta; }

		/**
			 * 
			 * @param Alpha 0-1 normalized range on the arc
			 * @return 
			 */
		FVector GetLocationOnArc(const double Alpha) const;
	};

	PCGEXCORE_API bool IsPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon);
	PCGEXCORE_API bool IsPointInPolygon(const FVector& Point, const TArray<FVector2D>& Polygon);

	PCGEXCORE_API bool IsAnyPointInPolygon(const TArray<FVector2D>& Points, const TArray<FVector2D>& Polygon);

	// L1/L∞ Voronoi edge path computation

	/** Transform 2D coordinates for L1/L∞ Voronoi computation: (x,y) -> (x+y, x-y) */
	FORCEINLINE static FVector2D TransformToLInf(const FVector2D& P) { return FVector2D(P.X + P.Y, P.X - P.Y); }

	/** Inverse transform: (u,v) -> ((u+v)/2, (u-v)/2) */
	FORCEINLINE static FVector2D TransformFromLInf(const FVector2D& P) { return FVector2D((P.X + P.Y) * 0.5, (P.X - P.Y) * 0.5); }

	/**
	 * Compute the edge path between two Voronoi cell centers for L∞ metric.
	 * L∞ edges are axis-aligned or 45° diagonal, with at most one bend.
	 * @param Start Start position (2D)
	 * @param End End position (2D)
	 * @param OutPath Output path including start, optional bend point, and end
	 */
	PCGEXCORE_API void ComputeLInfEdgePath(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPath);

	/**
	 * Compute the edge path between two Voronoi cell centers for L1 metric.
	 * L1 edges are axis-aligned or 45° diagonal, with at most one bend.
	 * Uses coordinate transform to leverage L∞ computation.
	 * @param Start Start position (2D)
	 * @param End End position (2D)
	 * @param OutPath Output path including start, optional bend point, and end
	 */
	PCGEXCORE_API void ComputeL1EdgePath(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPath);
}

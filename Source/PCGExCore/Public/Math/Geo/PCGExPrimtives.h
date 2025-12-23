// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExH.h"
#include "Math/Geo/PCGExGeo.h"

namespace PCGExMath::Geo
{
	struct PCGEXCORE_API FTriangle
	{
		int32 Vtx[3];

		explicit FTriangle(const int32 A, const int32 B, const int32 C)
		{
			Vtx[0] = A;
			Vtx[1] = B;
			Vtx[2] = C;
			Algo::Sort(Vtx);
		}

		explicit FTriangle(const int (&ABC)[3])
			: FTriangle(ABC[0], ABC[1], ABC[2])
		{
		}

		FORCEINLINE void Set(const int (&ABC)[3])
		{
			Vtx[0] = ABC[0];
			Vtx[1] = ABC[1];
			Vtx[2] = ABC[2];
		}

		FORCEINLINE void Remap(const TArrayView<const int32> Map)
		{
			Vtx[0] = Map[Vtx[0]];
			Vtx[1] = Map[Vtx[1]];
			Vtx[2] = Map[Vtx[2]];
		}

		FORCEINLINE bool Equals(const int32 A, const int32 B, const int32 C) const
		{
			return Vtx[0] == A && Vtx[1] == B && Vtx[2] == C;
		}

		FORCEINLINE bool ContainsEdge(const uint64 Edge) const
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Edge, A, B);
			return (Vtx[0] == A && Vtx[1] == B) || (Vtx[1] == A && Vtx[2] == B) || (Vtx[0] == A && Vtx[2] == B);
		}

		FORCEINLINE bool ContainsEdge(const int32 A, const int32 B) const
		{
			return (Vtx[0] == A && Vtx[1] == B) || (Vtx[1] == A && Vtx[2] == B) || (Vtx[0] == A && Vtx[2] == B);
		}

		FORCEINLINE void GetLongestEdge(const TArrayView<const FVector>& Positions, uint64& Edge) const
		{
			const double L[3] = {FVector::DistSquared(Positions[Vtx[0]], Positions[Vtx[1]]), FVector::DistSquared(Positions[Vtx[0]], Positions[Vtx[2]]), FVector::DistSquared(Positions[Vtx[1]], Positions[Vtx[2]])};

			if (L[0] > L[1] && L[0] > L[2]) { Edge = PCGEx::H64U(L[0], L[1]); }
			else if (L[1] > L[0] && L[1] > L[2]) { Edge = PCGEx::H64U(L[0], L[2]); }
			else { Edge = PCGEx::H64U(L[1], L[2]); }
		}

		FORCEINLINE void GetLongestEdge(const TArrayView<const FVector2D>& Positions, uint64& Edge) const
		{
			const double L[3] = {FVector2D::DistSquared(Positions[Vtx[0]], Positions[Vtx[1]]), FVector2D::DistSquared(Positions[Vtx[0]], Positions[Vtx[2]]), FVector2D::DistSquared(Positions[Vtx[1]], Positions[Vtx[2]])};

			if (L[0] > L[1] && L[0] > L[2]) { Edge = PCGEx::H64U(L[0], L[1]); }
			else if (L[1] > L[0] && L[1] > L[2]) { Edge = PCGEx::H64U(L[0], L[2]); }
			else { Edge = PCGEx::H64U(L[1], L[2]); }
		}

		FORCEINLINE void GetBounds(const TArrayView<const FVector>& Positions, FBox& Bounds) const
		{
			Bounds = FBox(ForceInit);
			Bounds += Positions[Vtx[0]];
			Bounds += Positions[Vtx[1]];
			Bounds += Positions[Vtx[2]];
		}

		FORCEINLINE void GetBounds(const TArrayView<const FVector2D>& Positions, FBox& Bounds) const
		{
			Bounds = FBox(ForceInit);
			Bounds += FVector(Positions[Vtx[0]], 0);
			Bounds += FVector(Positions[Vtx[1]], 0);
			Bounds += FVector(Positions[Vtx[2]], 0);
		}

		bool operator==(const FTriangle& Other) const
		{
			return Vtx[0] == Other.Vtx[0] && Vtx[1] == Other.Vtx[1] && Vtx[2] == Other.Vtx[2];
		}

		FORCEINLINE bool ContainsPoint(const FVector& P, const TArrayView<const FVector> Positions) const
		{
			return IsPointInTriangle(P, Positions[Vtx[0]], Positions[Vtx[1]], Positions[Vtx[2]]);
		}

		FORCEINLINE void FixWinding(const TArrayView<const FVector2D>& Positions)
		{
			const FVector2D& A = Positions[Vtx[0]];
			const FVector2D& B = Positions[Vtx[1]];
			const FVector2D& C = Positions[Vtx[2]];
			if ((B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y) > 0) { Swap(Vtx[1], Vtx[2]); }
		}

		template <typename T>
		FORCEINLINE bool IsConvex(const TArrayView<const T> Positions)
		{
			const FVector& V = Positions[Vtx[1]];
			return AngleCCW(Positions[Vtx[2]] - V, Positions[Vtx[0]] - V) > PI;
		}


		FORCEINLINE void FixWinding(const TArrayView<const FVector>& Positions, const FVector& Up = FVector::UpVector)
		{
			FixWinding(Positions[Vtx[0]], Positions[Vtx[1]], Positions[Vtx[2]], Up);
		}

		FORCEINLINE void FixWinding(const FVector& A, const FVector& B, const FVector& C, const FVector& Up = FVector::UpVector)
		{
			if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Up) > 0) { Swap(Vtx[1], Vtx[2]); }
		}
	};

	struct PCGEXCORE_API FBoundedTriangle : FTriangle
	{
		FBox Bounds;

		explicit FBoundedTriangle(const int32 A, const int32 B, const int32 C)
			: FTriangle(A, B, C)
		{
		}

		explicit FBoundedTriangle(const int (&ABC)[3])
			: FTriangle(ABC[0], ABC[1], ABC[2])
		{
		}

		FORCEINLINE void ComputeBounds(const TArrayView<FVector>& Positions) { GetBounds(Positions, Bounds); }
		FORCEINLINE void ComputeBounds(const TArrayView<FVector2D>& Positions) { GetBounds(Positions, Bounds); }
	};
}

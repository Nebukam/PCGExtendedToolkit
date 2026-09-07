// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/OBB/PCGExOBBTests.h"

namespace PCGExMath::OBB
{
	bool SATOverlap(const FOBB& A, const FOBB& B)
	{
		// Separating Axis Theorem (SAT) test for two oriented bounding boxes.
		// Tests 15 potential separating axes: 3 from each OBB's local axes + 9 cross products.
		// If any axis separates the projections, the OBBs don't overlap.
		// The KINDA_SMALL_NUMBER epsilon in AbsR prevents degenerate cross products
		// from causing false negatives when edges are nearly parallel.
		const FVector D = B.Bounds.Origin - A.Bounds.Origin;

		const FVector AxesA[3] = {A.Orientation.GetAxisX(), A.Orientation.GetAxisY(), A.Orientation.GetAxisZ()};
		const FVector AxesB[3] = {B.Orientation.GetAxisX(), B.Orientation.GetAxisY(), B.Orientation.GetAxisZ()};
		const FVector& EA = A.Bounds.Extents;
		const FVector& EB = B.Bounds.Extents;

		float R[3][3]; // Rotation matrix expressing B in A's frame
		float AbsR[3][3];

		for (int32 i = 0; i < 3; i++)
		{
			for (int32 j = 0; j < 3; j++)
			{
				R[i][j] = AxesA[i] | AxesB[j];
				AbsR[i][j] = FMath::Abs(R[i][j]) + KINDA_SMALL_NUMBER;
			}
		}

		// Test A's axes
		for (int32 i = 0; i < 3; i++)
		{
			const float ra = EA[i];
			const float rb = EB.X * AbsR[i][0] + EB.Y * AbsR[i][1] + EB.Z * AbsR[i][2];
			if (FMath::Abs(D | AxesA[i]) > ra + rb)
			{
				return false;
			}
		}

		// Test B's axes
		for (int32 i = 0; i < 3; i++)
		{
			const float ra = EA.X * AbsR[0][i] + EA.Y * AbsR[1][i] + EA.Z * AbsR[2][i];
			const float rb = EB[i];
			if (FMath::Abs(D | AxesB[i]) > ra + rb)
			{
				return false;
			}
		}

		// Test cross products (9 axes)
		// A0 x B0
		{
			const float ra = EA.Y * AbsR[2][0] + EA.Z * AbsR[1][0];
			const float rb = EB.Y * AbsR[0][2] + EB.Z * AbsR[0][1];
			if (FMath::Abs((D | AxesA[2]) * R[1][0] - (D | AxesA[1]) * R[2][0]) > ra + rb)
			{
				return false;
			}
		}
		// A0 x B1
		{
			const float ra = EA.Y * AbsR[2][1] + EA.Z * AbsR[1][1];
			const float rb = EB.X * AbsR[0][2] + EB.Z * AbsR[0][0];
			if (FMath::Abs((D | AxesA[2]) * R[1][1] - (D | AxesA[1]) * R[2][1]) > ra + rb)
			{
				return false;
			}
		}
		// A0 x B2
		{
			const float ra = EA.Y * AbsR[2][2] + EA.Z * AbsR[1][2];
			const float rb = EB.X * AbsR[0][1] + EB.Y * AbsR[0][0];
			if (FMath::Abs((D | AxesA[2]) * R[1][2] - (D | AxesA[1]) * R[2][2]) > ra + rb)
			{
				return false;
			}
		}
		// A1 x B0
		{
			const float ra = EA.X * AbsR[2][0] + EA.Z * AbsR[0][0];
			const float rb = EB.Y * AbsR[1][2] + EB.Z * AbsR[1][1];
			if (FMath::Abs((D | AxesA[0]) * R[2][0] - (D | AxesA[2]) * R[0][0]) > ra + rb)
			{
				return false;
			}
		}
		// A1 x B1
		{
			const float ra = EA.X * AbsR[2][1] + EA.Z * AbsR[0][1];
			const float rb = EB.X * AbsR[1][2] + EB.Z * AbsR[1][0];
			if (FMath::Abs((D | AxesA[0]) * R[2][1] - (D | AxesA[2]) * R[0][1]) > ra + rb)
			{
				return false;
			}
		}
		// A1 x B2
		{
			const float ra = EA.X * AbsR[2][2] + EA.Z * AbsR[0][2];
			const float rb = EB.X * AbsR[1][1] + EB.Y * AbsR[1][0];
			if (FMath::Abs((D | AxesA[0]) * R[2][2] - (D | AxesA[2]) * R[0][2]) > ra + rb)
			{
				return false;
			}
		}
		// A2 x B0
		{
			const float ra = EA.X * AbsR[1][0] + EA.Y * AbsR[0][0];
			const float rb = EB.Y * AbsR[2][2] + EB.Z * AbsR[2][1];
			if (FMath::Abs((D | AxesA[1]) * R[0][0] - (D | AxesA[0]) * R[1][0]) > ra + rb)
			{
				return false;
			}
		}
		// A2 x B1
		{
			const float ra = EA.X * AbsR[1][1] + EA.Y * AbsR[0][1];
			const float rb = EB.X * AbsR[2][2] + EB.Z * AbsR[2][0];
			if (FMath::Abs((D | AxesA[1]) * R[0][1] - (D | AxesA[0]) * R[1][1]) > ra + rb)
			{
				return false;
			}
		}
		// A2 x B2
		{
			const float ra = EA.X * AbsR[1][2] + EA.Y * AbsR[0][2];
			const float rb = EB.X * AbsR[2][1] + EB.Y * AbsR[2][0];
			if (FMath::Abs((D | AxesA[1]) * R[0][2] - (D | AxesA[0]) * R[1][2]) > ra + rb)
			{
				return false;
			}
		}

		return true;
	}

	bool TriangleOverlap(const FOBB& Box, const FVector& A, const FVector& B, const FVector& C)
	{
		// Akenine-Möller triangle/AABB test once the triangle is in the box's frame:
		// 3 box axes, the triangle normal, then the 9 edge x axis cross products.
		const FVector& E = Box.Bounds.Extents;
		const FVector V0 = Box.ToLocal(A);
		const FVector V1 = Box.ToLocal(B);
		const FVector V2 = Box.ToLocal(C);

		if (FMath::Max3(V0.X, V1.X, V2.X) < -E.X || FMath::Min3(V0.X, V1.X, V2.X) > E.X) { return false; }
		if (FMath::Max3(V0.Y, V1.Y, V2.Y) < -E.Y || FMath::Min3(V0.Y, V1.Y, V2.Y) > E.Y) { return false; }
		if (FMath::Max3(V0.Z, V1.Z, V2.Z) < -E.Z || FMath::Min3(V0.Z, V1.Z, V2.Z) > E.Z) { return false; }

		const FVector F0 = V1 - V0;
		const FVector F1 = V2 - V1;
		const FVector F2 = V0 - V2;

		const FVector N = F0 ^ F1;
		if (FMath::Abs(N | V0) > E.X * FMath::Abs(N.X) + E.Y * FMath::Abs(N.Y) + E.Z * FMath::Abs(N.Z)) { return false; }

		auto Separated = [&](const FVector& Axis) -> bool
		{
			const double P0 = Axis | V0;
			const double P1 = Axis | V1;
			const double P2 = Axis | V2;
			const double R = E.X * FMath::Abs(Axis.X) + E.Y * FMath::Abs(Axis.Y) + E.Z * FMath::Abs(Axis.Z);
			return FMath::Max(-FMath::Max3(P0, P1, P2), FMath::Min3(P0, P1, P2)) > R;
		};

		const FVector Edges[3] = {F0, F1, F2};
		for (const FVector& F : Edges)
		{
			// Unit axis x F, written out: X^F, Y^F, Z^F
			if (Separated(FVector(0, -F.Z, F.Y)) || Separated(FVector(F.Z, 0, -F.X)) || Separated(FVector(-F.Y, F.X, 0)))
			{
				return false;
			}
		}

		return true;
	}

	float SATPenetrationDepth(const FOBB& A, const FOBB& B)
	{
		// Same structure as SATOverlap but tracks minimum overlap across all 15 axes.
		// Returns positive depth if overlapping (MTV magnitude), negative if separated.
		const FVector D = B.Bounds.Origin - A.Bounds.Origin;

		const FVector AxesA[3] = {A.Orientation.GetAxisX(), A.Orientation.GetAxisY(), A.Orientation.GetAxisZ()};
		const FVector AxesB[3] = {B.Orientation.GetAxisX(), B.Orientation.GetAxisY(), B.Orientation.GetAxisZ()};
		const FVector& EA = A.Bounds.Extents;
		const FVector& EB = B.Bounds.Extents;

		float R[3][3];
		float AbsR[3][3];

		for (int32 i = 0; i < 3; i++)
		{
			for (int32 j = 0; j < 3; j++)
			{
				R[i][j] = AxesA[i] | AxesB[j];
				AbsR[i][j] = FMath::Abs(R[i][j]) + KINDA_SMALL_NUMBER;
			}
		}

		float MinOverlap = TNumericLimits<float>::Max();

		// Test A's axes
		for (int32 i = 0; i < 3; i++)
		{
			const float ra = EA[i];
			const float rb = EB.X * AbsR[i][0] + EB.Y * AbsR[i][1] + EB.Z * AbsR[i][2];
			const float Overlap = (ra + rb) - FMath::Abs(D | AxesA[i]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}

		// Test B's axes
		for (int32 i = 0; i < 3; i++)
		{
			const float ra = EA.X * AbsR[0][i] + EA.Y * AbsR[1][i] + EA.Z * AbsR[2][i];
			const float rb = EB[i];
			const float Overlap = (ra + rb) - FMath::Abs(D | AxesB[i]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}

		// Test cross products (9 axes)
		// A0 x B0
		{
			const float ra = EA.Y * AbsR[2][0] + EA.Z * AbsR[1][0];
			const float rb = EB.Y * AbsR[0][2] + EB.Z * AbsR[0][1];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[2]) * R[1][0] - (D | AxesA[1]) * R[2][0]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A0 x B1
		{
			const float ra = EA.Y * AbsR[2][1] + EA.Z * AbsR[1][1];
			const float rb = EB.X * AbsR[0][2] + EB.Z * AbsR[0][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[2]) * R[1][1] - (D | AxesA[1]) * R[2][1]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A0 x B2
		{
			const float ra = EA.Y * AbsR[2][2] + EA.Z * AbsR[1][2];
			const float rb = EB.X * AbsR[0][1] + EB.Y * AbsR[0][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[2]) * R[1][2] - (D | AxesA[1]) * R[2][2]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A1 x B0
		{
			const float ra = EA.X * AbsR[2][0] + EA.Z * AbsR[0][0];
			const float rb = EB.Y * AbsR[1][2] + EB.Z * AbsR[1][1];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[0]) * R[2][0] - (D | AxesA[2]) * R[0][0]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A1 x B1
		{
			const float ra = EA.X * AbsR[2][1] + EA.Z * AbsR[0][1];
			const float rb = EB.X * AbsR[1][2] + EB.Z * AbsR[1][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[0]) * R[2][1] - (D | AxesA[2]) * R[0][1]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A1 x B2
		{
			const float ra = EA.X * AbsR[2][2] + EA.Z * AbsR[0][2];
			const float rb = EB.X * AbsR[1][1] + EB.Y * AbsR[1][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[0]) * R[2][2] - (D | AxesA[2]) * R[0][2]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A2 x B0
		{
			const float ra = EA.X * AbsR[1][0] + EA.Y * AbsR[0][0];
			const float rb = EB.Y * AbsR[2][2] + EB.Z * AbsR[2][1];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[1]) * R[0][0] - (D | AxesA[0]) * R[1][0]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A2 x B1
		{
			const float ra = EA.X * AbsR[1][1] + EA.Y * AbsR[0][1];
			const float rb = EB.X * AbsR[2][2] + EB.Z * AbsR[2][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[1]) * R[0][1] - (D | AxesA[0]) * R[1][1]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}
		// A2 x B2
		{
			const float ra = EA.X * AbsR[1][2] + EA.Y * AbsR[0][2];
			const float rb = EB.X * AbsR[2][1] + EB.Y * AbsR[2][0];
			const float Overlap = (ra + rb) - FMath::Abs((D | AxesA[1]) * R[0][2] - (D | AxesA[0]) * R[1][2]);
			if (Overlap < 0)
			{
				return Overlap;
			}
			MinOverlap = FMath::Min(MinOverlap, Overlap);
		}

		return MinOverlap;
	}
}

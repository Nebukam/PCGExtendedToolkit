// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/OBB/PCGExOBBTests.h"

namespace PCGExMath::OBB
{
	bool SATOverlap(const FOBB& A, const FOBB& B)
	{
		const FVector D = B.Bounds.Origin - A.Bounds.Origin;

		const FVector AxesA[3] = { A.Orientation.GetAxisX(), A.Orientation.GetAxisY(), A.Orientation.GetAxisZ() };
		const FVector AxesB[3] = { B.Orientation.GetAxisX(), B.Orientation.GetAxisY(), B.Orientation.GetAxisZ() };
		const FVector& EA = A.Bounds.HalfExtents;
		const FVector& EB = B.Bounds.HalfExtents;

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
			if (FMath::Abs(D | AxesA[i]) > ra + rb) return false;
		}

		// Test B's axes
		for (int32 i = 0; i < 3; i++)
		{
			const float ra = EA.X * AbsR[0][i] + EA.Y * AbsR[1][i] + EA.Z * AbsR[2][i];
			const float rb = EB[i];
			if (FMath::Abs(D | AxesB[i]) > ra + rb) return false;
		}

		// Test cross products (9 axes)
		// A0 x B0
		{
			const float ra = EA.Y * AbsR[2][0] + EA.Z * AbsR[1][0];
			const float rb = EB.Y * AbsR[0][2] + EB.Z * AbsR[0][1];
			if (FMath::Abs((D | AxesA[2]) * R[1][0] - (D | AxesA[1]) * R[2][0]) > ra + rb) return false;
		}
		// A0 x B1
		{
			const float ra = EA.Y * AbsR[2][1] + EA.Z * AbsR[1][1];
			const float rb = EB.X * AbsR[0][2] + EB.Z * AbsR[0][0];
			if (FMath::Abs((D | AxesA[2]) * R[1][1] - (D | AxesA[1]) * R[2][1]) > ra + rb) return false;
		}
		// A0 x B2
		{
			const float ra = EA.Y * AbsR[2][2] + EA.Z * AbsR[1][2];
			const float rb = EB.X * AbsR[0][1] + EB.Y * AbsR[0][0];
			if (FMath::Abs((D | AxesA[2]) * R[1][2] - (D | AxesA[1]) * R[2][2]) > ra + rb) return false;
		}
		// A1 x B0
		{
			const float ra = EA.X * AbsR[2][0] + EA.Z * AbsR[0][0];
			const float rb = EB.Y * AbsR[1][2] + EB.Z * AbsR[1][1];
			if (FMath::Abs((D | AxesA[0]) * R[2][0] - (D | AxesA[2]) * R[0][0]) > ra + rb) return false;
		}
		// A1 x B1
		{
			const float ra = EA.X * AbsR[2][1] + EA.Z * AbsR[0][1];
			const float rb = EB.X * AbsR[1][2] + EB.Z * AbsR[1][0];
			if (FMath::Abs((D | AxesA[0]) * R[2][1] - (D | AxesA[2]) * R[0][1]) > ra + rb) return false;
		}
		// A1 x B2
		{
			const float ra = EA.X * AbsR[2][2] + EA.Z * AbsR[0][2];
			const float rb = EB.X * AbsR[1][1] + EB.Y * AbsR[1][0];
			if (FMath::Abs((D | AxesA[0]) * R[2][2] - (D | AxesA[2]) * R[0][2]) > ra + rb) return false;
		}
		// A2 x B0
		{
			const float ra = EA.X * AbsR[1][0] + EA.Y * AbsR[0][0];
			const float rb = EB.Y * AbsR[2][2] + EB.Z * AbsR[2][1];
			if (FMath::Abs((D | AxesA[1]) * R[0][0] - (D | AxesA[0]) * R[1][0]) > ra + rb) return false;
		}
		// A2 x B1
		{
			const float ra = EA.X * AbsR[1][1] + EA.Y * AbsR[0][1];
			const float rb = EB.X * AbsR[2][2] + EB.Z * AbsR[2][0];
			if (FMath::Abs((D | AxesA[1]) * R[0][1] - (D | AxesA[0]) * R[1][1]) > ra + rb) return false;
		}
		// A2 x B2
		{
			const float ra = EA.X * AbsR[1][2] + EA.Y * AbsR[0][2];
			const float rb = EB.X * AbsR[2][1] + EB.Y * AbsR[2][0];
			if (FMath::Abs((D | AxesA[1]) * R[0][2] - (D | AxesA[0]) * R[1][2]) > ra + rb) return false;
		}

		return true;
	}
}

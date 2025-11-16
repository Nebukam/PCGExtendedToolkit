// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExDetailsAxis.h"

namespace PCGEx
{
	void GetAxesOrder(EPCGExMakeRotAxis Order, int32& A, int32& B, int32& C)
	{
		switch (Order)
		{
		case EPCGExMakeRotAxis::X:
		case EPCGExMakeRotAxis::XY:
			A = 0;
			B = 1;
			C = 2;
			break;
		case EPCGExMakeRotAxis::XZ:
			A = 0;
			B = 2;
			C = 1;
			break;
		case EPCGExMakeRotAxis::Y:
		case EPCGExMakeRotAxis::YX:
			A = 1;
			B = 0;
			C = 2;
			break;
		case EPCGExMakeRotAxis::YZ:
			A = 1;
			B = 2;
			C = 0;
			break;
		case EPCGExMakeRotAxis::Z:
		case EPCGExMakeRotAxis::ZX:
			A = 2;
			B = 0;
			C = 1;
			break;
		case EPCGExMakeRotAxis::ZY:
			A = 2;
			B = 1;
			C = 0;
			break;
		}
	}

	FQuat MakeRot(const EPCGExMakeRotAxis Order, const FVector& X, const FVector& Y, const FVector& Z)
	{
		switch (Order)
		{
		default:
		case EPCGExMakeRotAxis::X: return FRotationMatrix::MakeFromX(X).ToQuat();
		case EPCGExMakeRotAxis::XY: return FRotationMatrix::MakeFromXY(X, Y).ToQuat();
		case EPCGExMakeRotAxis::XZ: return FRotationMatrix::MakeFromXZ(X, Z).ToQuat();
		case EPCGExMakeRotAxis::Y: return FRotationMatrix::MakeFromY(Y).ToQuat();
		case EPCGExMakeRotAxis::YX: return FRotationMatrix::MakeFromYX(Y, X).ToQuat();
		case EPCGExMakeRotAxis::YZ: return FRotationMatrix::MakeFromYZ(Y, Z).ToQuat();
		case EPCGExMakeRotAxis::Z: return FRotationMatrix::MakeFromZ(Z).ToQuat();
		case EPCGExMakeRotAxis::ZX: return FRotationMatrix::MakeFromZX(Z, X).ToQuat();
		case EPCGExMakeRotAxis::ZY: return FRotationMatrix::MakeFromZY(Z, Y).ToQuat();
		}
	}

	void FindOrderMatch(
		const FQuat& Quat,
		const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
		int32& X, int32& Y, int32& Z, const bool bPermute)
	{
		const FVector QA[3] = { Quat.GetAxisX(), Quat.GetAxisY(), Quat.GetAxisZ() };

		double M[3][3];
		for (int i = 0; i < 3; ++i)
		{
			M[i][0] = FMath::Abs(FVector::DotProduct(QA[i], XAxis));
			M[i][1] = FMath::Abs(FVector::DotProduct(QA[i], YAxis));
			M[i][2] = FMath::Abs(FVector::DotProduct(QA[i], ZAxis));
		}

		// choose best X/Y/Z directly
		int32 Best[3];
		for (int i = 0; i < 3; ++i)
		{
			const double DX = M[i][0];
			const double DY = M[i][1];
			const double DZ = M[i][2];
			Best[i] = (DX >= DY && DX >= DZ) ? 0 : ((DY >= DZ) ? 1 : 2);
		}

		if (!bPermute)
		{
			X = Best[0];
			Y = Best[1];
			Z = Best[2];
			return;
		}

		// guaranteed permutation with constant-time deterministic resolution
		static const int32 Permutations[6][3] =
		{
			{0,1,2}, {0,2,1},
			{1,0,2}, {1,2,0},
			{2,0,1}, {2,1,0}
		};

		int32 BestScore = -1;
		const int32* BestPerm = Permutations[0];

		for (int p = 0; p < 6; ++p)
		{
			const int32* P = Permutations[p];
			const double Score =
				M[0][P[0]] +
				M[1][P[1]] +
				M[2][P[2]];

			if (Score > BestScore)
			{
				BestScore = Score;
				BestPerm = P;
			}
		}

		X = BestPerm[0];
		Y = BestPerm[1];
		Z = BestPerm[2];

		check(X >= 0 && X <= 2);
		check(Y >= 0 && Y <= 2);
		check(Z >= 0 && Z <= 2);
	}
}

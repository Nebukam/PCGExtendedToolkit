// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExMathAxis.h"

namespace PCGExMath
{
	void GetAxesOrder(EPCGExMakeRotAxis Order, int32& A, int32& B, int32& C)
	{
		switch (Order)
		{
		case EPCGExMakeRotAxis::X:
		case EPCGExMakeRotAxis::XY: A = 0;
			B = 1;
			C = 2;
			break;
		case EPCGExMakeRotAxis::XZ: A = 0;
			B = 2;
			C = 1;
			break;
		case EPCGExMakeRotAxis::Y:
		case EPCGExMakeRotAxis::YX: A = 1;
			B = 0;
			C = 2;
			break;
		case EPCGExMakeRotAxis::YZ: A = 1;
			B = 2;
			C = 0;
			break;
		case EPCGExMakeRotAxis::Z:
		case EPCGExMakeRotAxis::ZX: A = 2;
			B = 0;
			C = 1;
			break;
		case EPCGExMakeRotAxis::ZY: A = 2;
			B = 1;
			C = 0;
			break;
		}
	}

	FQuat MakeRot(const EPCGExMakeRotAxis Order, const FVector& X, const FVector& Y, const FVector& Z)
	{
		switch (Order)
		{
		default: case EPCGExMakeRotAxis::X: return FRotationMatrix::MakeFromX(X).ToQuat();
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

	FQuat MakeRot(const EPCGExMakeRotAxis Order, const FVector& A, const FVector& B)
	{
		switch (Order)
		{
		default: case EPCGExMakeRotAxis::X: return FRotationMatrix::MakeFromX(A).ToQuat();
		case EPCGExMakeRotAxis::XY: return FRotationMatrix::MakeFromXY(A, B).ToQuat();
		case EPCGExMakeRotAxis::XZ: return FRotationMatrix::MakeFromXZ(A, B).ToQuat();
		case EPCGExMakeRotAxis::Y: return FRotationMatrix::MakeFromY(A).ToQuat();
		case EPCGExMakeRotAxis::YX: return FRotationMatrix::MakeFromYX(A, B).ToQuat();
		case EPCGExMakeRotAxis::YZ: return FRotationMatrix::MakeFromYZ(A, B).ToQuat();
		case EPCGExMakeRotAxis::Z: return FRotationMatrix::MakeFromZ(A).ToQuat();
		case EPCGExMakeRotAxis::ZX: return FRotationMatrix::MakeFromZX(A, B).ToQuat();
		case EPCGExMakeRotAxis::ZY: return FRotationMatrix::MakeFromZY(B, B).ToQuat();
		}
	}

	void FindOrderMatch(const FQuat& Quat, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, int32& X, int32& Y, int32& Z, const bool bPermute)
	{
		const FVector QA[3] = {Quat.GetAxisX(), Quat.GetAxisY(), Quat.GetAxisZ()};

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
		static const int32 Permutations[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};

		int32 BestScore = -1;
		const int32* BestPerm = Permutations[0];

		for (int p = 0; p < 6; ++p)
		{
			const int32* P = Permutations[p];
			const double Score = M[0][P[0]] + M[1][P[1]] + M[2][P[2]];

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

	FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default: case EPCGExAxis::Forward: return GetDirection<EPCGExAxis::Forward>(Quat);
		case EPCGExAxis::Backward: return GetDirection<EPCGExAxis::Backward>(Quat);
		case EPCGExAxis::Right: return GetDirection<EPCGExAxis::Right>(Quat);
		case EPCGExAxis::Left: return GetDirection<EPCGExAxis::Left>(Quat);
		case EPCGExAxis::Up: return GetDirection<EPCGExAxis::Up>(Quat);
		case EPCGExAxis::Down: return GetDirection<EPCGExAxis::Down>(Quat);
		}
	}

	FVector GetDirection(const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default: case EPCGExAxis::Forward: return PCGEX_AXIS_X;
		case EPCGExAxis::Backward: return PCGEX_AXIS_X_N;
		case EPCGExAxis::Right: return PCGEX_AXIS_Y;
		case EPCGExAxis::Left: return PCGEX_AXIS_Y_N;
		case EPCGExAxis::Up: return PCGEX_AXIS_Z;
		case EPCGExAxis::Down: return PCGEX_AXIS_Z_N;
		}
	}

	FTransform GetIdentity(const EPCGExAxisOrder Order)
	{
		switch (Order)
		{
		default: case EPCGExAxisOrder::XYZ: return FTransform(FMatrix(PCGEX_AXIS_X, PCGEX_AXIS_Y, PCGEX_AXIS_Z, FVector::Zero()));
		case EPCGExAxisOrder::YZX: return FTransform(FMatrix(PCGEX_AXIS_Y, PCGEX_AXIS_Z, PCGEX_AXIS_X, FVector::Zero()));
		case EPCGExAxisOrder::ZXY: return FTransform(FMatrix(PCGEX_AXIS_Z, PCGEX_AXIS_X, PCGEX_AXIS_Y, FVector::Zero()));
		case EPCGExAxisOrder::YXZ: return FTransform(FMatrix(PCGEX_AXIS_Y, PCGEX_AXIS_X, PCGEX_AXIS_Z, FVector::Zero()));
		case EPCGExAxisOrder::ZYX: return FTransform(FMatrix(PCGEX_AXIS_Z, PCGEX_AXIS_Y, PCGEX_AXIS_X, FVector::Zero()));
		case EPCGExAxisOrder::XZY: return FTransform(FMatrix(PCGEX_AXIS_X, PCGEX_AXIS_Z, PCGEX_AXIS_Y, FVector::Zero()));
		}
	}

	void Swizzle(FVector& Vector, const EPCGExAxisOrder Order)
	{
		int32 A;
		int32 B;
		int32 C;
		GetAxesOrder(Order, A, B, C);
		FVector Temp = Vector;
		Vector[0] = Temp[A];
		Vector[1] = Temp[B];
		Vector[2] = Temp[C];
	}

	void Swizzle(FVector& Vector, const int32 (&Order)[3])
	{
		FVector Temp = Vector;
		Vector[0] = Temp[Order[0]];
		Vector[1] = Temp[Order[1]];
		Vector[2] = Temp[Order[2]];
	}

	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward)
	{
		switch (Dir)
		{
		default: case EPCGExAxis::Forward: return FRotationMatrix::MakeFromX(InForward * -1).ToQuat();
		case EPCGExAxis::Backward: return FRotationMatrix::MakeFromX(InForward).ToQuat();
		case EPCGExAxis::Right: return FRotationMatrix::MakeFromY(InForward * -1).ToQuat();
		case EPCGExAxis::Left: return FRotationMatrix::MakeFromY(InForward).ToQuat();
		case EPCGExAxis::Up: return FRotationMatrix::MakeFromZ(InForward * -1).ToQuat();
		case EPCGExAxis::Down: return FRotationMatrix::MakeFromZ(InForward).ToQuat();
		}
	}

	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp)
	{
		switch (Dir)
		{
		default: case EPCGExAxis::Forward: return FRotationMatrix::MakeFromXZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Backward: return FRotationMatrix::MakeFromXZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Right: return FRotationMatrix::MakeFromYZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Left: return FRotationMatrix::MakeFromYZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Up: return FRotationMatrix::MakeFromZY(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Down: return FRotationMatrix::MakeFromZY(InForward, InUp).ToQuat();
		}
	}

	FVector GetNormal(const FVector& A, const FVector& B, const FVector& C)
	{
		return FVector::CrossProduct((B - A), (C - A)).GetSafeNormal();
	}

	FVector GetNormalUp(const FVector& A, const FVector& B, const FVector& Up)
	{
		return FVector::CrossProduct((B - A), ((B + Up) - A)).GetSafeNormal();
	}

	FTransform MakeLookAtTransform(const FVector& LookAt, const FVector& LookUp, const EPCGExAxisAlign AlignAxis)
	{
		switch (AlignAxis)
		{
		case EPCGExAxisAlign::Forward: return FTransform(FRotationMatrix::MakeFromXZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Backward: return FTransform(FRotationMatrix::MakeFromXZ(LookAt, LookUp));
		case EPCGExAxisAlign::Right: return FTransform(FRotationMatrix::MakeFromYZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Left: return FTransform(FRotationMatrix::MakeFromYZ(LookAt, LookUp));
		case EPCGExAxisAlign::Up: return FTransform(FRotationMatrix::MakeFromZY(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Down: return FTransform(FRotationMatrix::MakeFromZY(LookAt, LookUp));
		default: return FTransform::Identity;
		}
	}

	double GetAngle(const FVector& A, const FVector& B)
	{
		const FVector Cross = FVector::CrossProduct(A, B);
		const double Atan2 = FMath::Atan2(Cross.Size(), A.Dot(B));
		return Cross.Z < 0 ? TWO_PI - Atan2 : Atan2;
	}

	double GetRadiansBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector)
	{
		const double Radians = FMath::Acos(FVector::DotProduct(A, B));
		return FVector::CrossProduct(A, B).Z < 0 ? TWO_PI - Radians : Radians;
	}

	double GetRadiansBetweenVectors(const FVector2D& A, const FVector2D& B)
	{
		return GetRadiansBetweenVectors(FVector(A, 0), FVector(B, 0));
		//const double Radians = FMath::Atan2(FVector2D::CrossProduct(A, B), FVector2D::DotProduct(A, B));
		//return (Radians >= 0) ? Radians : (Radians + TWO_PI);
	}

	double GetDegreesBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector)
	{
		const double D = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(A, B)));
		return FVector::DotProduct(FVector::CrossProduct(A, B), UpVector) < 0 ? 360 - D : D;
	}
}

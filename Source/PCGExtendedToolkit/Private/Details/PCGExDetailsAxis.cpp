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
		const FVector QuatAxes[3] = { Quat.GetAxisX(), Quat.GetAxisY(), Quat.GetAxisZ() };

		auto FindBest = [&](const FVector& Ref)
		{
			const double DX = FMath::Abs(FVector::DotProduct(XAxis, Ref));
			const double DY = FMath::Abs(FVector::DotProduct(YAxis, Ref));
			const double DZ = FMath::Abs(FVector::DotProduct(ZAxis, Ref));
			return (DX > DY && DX > DZ) ? 0 : (DY > DZ ? 1 : 2);
		};

		X = FindBest(QuatAxes[0]);
		Y = FindBest(QuatAxes[1]);
		Z = FindBest(QuatAxes[2]);

		if (bPermute)
		{
			const int OldX = X;
			const int OldY = Y;
			const int OldZ = Z;

			uint8 Inv[3];
			Inv[OldX] = 0;
			Inv[OldY] = 1;
			Inv[OldZ] = 2;

			X = Inv[0];
			Y = Inv[1];
			Z = Inv[2];
		}
	}
}

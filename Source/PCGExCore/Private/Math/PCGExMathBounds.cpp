// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExMathBounds.h"
#include "Data/PCGExData.h"
#include "Utils/PCGValueRange.h"

namespace PCGExMath
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
		if (FMath::Abs(T0) > EA0 + EB0 * AR00 + EB1 * AR01 + EB2 * AR02) { return false; }
		if (FMath::Abs(T1) > EA1 + EB0 * AR10 + EB1 * AR11 + EB2 * AR12) { return false; }
		if (FMath::Abs(T2) > EA2 + EB0 * AR20 + EB1 * AR21 + EB2 * AR22) { return false; }

		// B's local axes (3 tests)
		if (FMath::Abs(T0 * R00 + T1 * R10 + T2 * R20) > EA0 * AR00 + EA1 * AR10 + EA2 * AR20 + EB0) { return false; }
		if (FMath::Abs(T0 * R01 + T1 * R11 + T2 * R21) > EA0 * AR01 + EA1 * AR11 + EA2 * AR21 + EB1) { return false; }
		if (FMath::Abs(T0 * R02 + T1 * R12 + T2 * R22) > EA0 * AR02 + EA1 * AR12 + EA2 * AR22 + EB2) { return false; }

		// Cross product axes (9 tests)
		if (FMath::Abs(T2 * R10 - T1 * R20) > EA1 * AR20 + EA2 * AR10 + EB1 * AR02 + EB2 * AR01) { return false; }
		if (FMath::Abs(T2 * R11 - T1 * R21) > EA1 * AR21 + EA2 * AR11 + EB0 * AR02 + EB2 * AR00) { return false; }
		if (FMath::Abs(T2 * R12 - T1 * R22) > EA1 * AR22 + EA2 * AR12 + EB0 * AR01 + EB1 * AR00) { return false; }
		if (FMath::Abs(T0 * R20 - T2 * R00) > EA0 * AR20 + EA2 * AR00 + EB1 * AR12 + EB2 * AR11) { return false; }
		if (FMath::Abs(T0 * R21 - T2 * R01) > EA0 * AR21 + EA2 * AR01 + EB0 * AR12 + EB2 * AR10) { return false; }
		if (FMath::Abs(T0 * R22 - T2 * R02) > EA0 * AR22 + EA2 * AR02 + EB0 * AR11 + EB1 * AR10) { return false; }
		if (FMath::Abs(T1 * R00 - T0 * R10) > EA0 * AR10 + EA1 * AR00 + EB1 * AR22 + EB2 * AR21) { return false; }
		if (FMath::Abs(T1 * R01 - T0 * R11) > EA0 * AR11 + EA1 * AR01 + EB0 * AR22 + EB2 * AR20) { return false; }
		if (FMath::Abs(T1 * R02 - T0 * R12) > EA0 * AR12 + EA1 * AR02 + EB0 * AR21 + EB1 * AR20) { return false; }

		return true;
	}

	FBox GetLocalBounds(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		case EPCGExPointBoundsSource::ScaledBounds: return GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point);
		case EPCGExPointBoundsSource::DensityBounds: return GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point);
		case EPCGExPointBoundsSource::Bounds: return GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point);
		case EPCGExPointBoundsSource::Center: return GetLocalBounds<EPCGExPointBoundsSource::Center>(Point);
		default: return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}

	FBox GetLocalBounds(const PCGExData::FProxyPoint& Point, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		case EPCGExPointBoundsSource::ScaledBounds: return GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point);
		case EPCGExPointBoundsSource::DensityBounds: return GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point);
		case EPCGExPointBoundsSource::Bounds: return GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point);
		case EPCGExPointBoundsSource::Center: return GetLocalBounds<EPCGExPointBoundsSource::Center>(Point);
		default: return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}

	FBox GetBounds(const TArrayView<FVector> InPositions)
	{
		FBox Bounds = FBox(ForceInit);
		for (const FVector& Position : InPositions) { Bounds += Position; }
		SanitizeBounds(Bounds);
		return Bounds;
	}

	FBox GetBounds(const TConstPCGValueRange<FTransform>& InTransforms)
	{
		FBox Bounds = FBox(ForceInit);
		for (const FTransform& Transform : InTransforms) { Bounds += Transform.GetLocation(); }
		SanitizeBounds(Bounds);
		return Bounds;
	}

	FBox GetBounds(const UPCGBasePointData* InPointData, const EPCGExPointBoundsSource Source)
	{
		FBox Bounds = FBox(ForceInit);
		const int32 NumPoints = InPointData->GetNumPoints();
		FTransform Transform;

		switch (Source)
		{
		case EPCGExPointBoundsSource::ScaledBounds:
			for (int i = 0; i < NumPoints; i++)
			{
				PCGExData::FConstPoint Point(InPointData, i);
				Point.GetTransformNoScale(Transform);
				Bounds += PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).TransformBy(Transform);
			}
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			for (int i = 0; i < NumPoints; i++)
			{
				PCGExData::FConstPoint Point(InPointData, i);
				Point.GetTransformNoScale(Transform);
				Bounds += PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point).TransformBy(Transform);
			}
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (int i = 0; i < NumPoints; i++)
			{
				PCGExData::FConstPoint Point(InPointData, i);
				Point.GetTransformNoScale(Transform);
				Bounds += PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point).TransformBy(Transform);
			}
			break;
		default: case EPCGExPointBoundsSource::Center:
			for (int i = 0; i < NumPoints; i++)
			{
				Bounds += PCGExData::FConstPoint(InPointData, i).GetLocation();
			}
			break;
		}

		SanitizeBounds(Bounds);
		return Bounds;
	}
}

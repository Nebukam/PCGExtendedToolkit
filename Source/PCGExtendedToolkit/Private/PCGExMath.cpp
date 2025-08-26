﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMath.h"
#include "PCGEx.h"
#include "PCGExMacros.h"

namespace PCGExMath
{
	FClosestPosition::FClosestPosition(const FVector& InOrigin)
		: Origin(InOrigin)
	{
	}

	FClosestPosition::FClosestPosition(const FVector& InOrigin, const FVector& InLocation)
		: bValid(true), Origin(InOrigin), Location(InLocation), DistSquared(FVector::DistSquared(InOrigin, InLocation))
	{
	}

	FClosestPosition::FClosestPosition(const FVector& InOrigin, const FVector& InLocation, const int32 InIndex)
		: bValid(true), Index(InIndex), Origin(InOrigin), Location(InLocation), DistSquared(FVector::DistSquared(InOrigin, InLocation))
	{
	}

	bool FClosestPosition::Update(const FVector& InLocation)
	{
		if (const double Dist = FVector::DistSquared(Origin, InLocation); Dist < DistSquared)
		{
			bValid = true;
			DistSquared = Dist;
			Location = InLocation;
			return true;
		}

		return false;
	}

	bool FClosestPosition::Update(const FVector& InLocation, const int32 InIndex)
	{
		if (Update(InLocation))
		{
			Index = InIndex;
			return true;
		}

		return false;
	}

	FSegment::FSegment(const FVector& InA, const FVector& InB, const double Expansion)
		: A(InA), B(InB), Direction((B - A).GetSafeNormal())
	{
		Bounds += A;
		Bounds += B;
		Bounds = Bounds.ExpandBy(Expansion);
	}

	bool FSegment::FindIntersection(const FVector& A2, const FVector& B2, double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const EIntersectionTestMode Mode) const
	{
		FMath::SegmentDistToSegment(A, B, A2, B2, OutSelf, OutOther);

		switch (Mode)
		{
		case EIntersectionTestMode::Loose:
			break;
		case EIntersectionTestMode::Strict:
			if (A == OutSelf || B == OutSelf || A2 == OutOther || B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::StrictOnSelfA:
			if (A == OutSelf) { return false; }
			break;
		case EIntersectionTestMode::StrictOnSelfB:
			if (B == OutSelf) { return false; }
			break;
		case EIntersectionTestMode::StrictOnOtherA:
			if (A2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::StrictOnOtherB:
			if (B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::LooseOnSelf:
			if (A2 == OutOther || B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::LooseOnSelfA:
			if (B == OutSelf || A2 == OutOther || B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::LooseOnSelfB:
			if (A == OutSelf || A2 == OutOther || B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::LooseOnOther:
			if (A == OutSelf || B == OutSelf) { return false; }
			break;
		case EIntersectionTestMode::LooseOnOtherA:
			if (A == OutSelf || B == OutSelf || B2 == OutOther) { return false; }
			break;
		case EIntersectionTestMode::LooseOnOtherB:
			if (A == OutSelf || B == OutSelf || A2 == OutOther) { return false; }
			break;
		}

		if (FVector::DistSquared(OutSelf, OutOther) >= SquaredTolerance) { return false; }
		return true;
	}

	bool FSegment::FindIntersection(const FSegment& S, double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const EIntersectionTestMode Mode) const
	{
		return FindIntersection(S.A, S.B, SquaredTolerance, OutSelf, OutOther, Mode);
	}

	double ConvertStringToDouble(const FString& StringToConvert)
	{
		const TCHAR* CharArray = *StringToConvert;
		const double Result = FCString::Atod(CharArray);
		return FMath::IsNaN(Result) ? 0 : Result;
	}

	double GetMode(const TArray<double>& Values, const bool bHighest, const uint32 Tolerance)
	{
		TMap<double, int32> Map;
		int32 LastCount = 0;
		double Mode = 0;

		for (const double Value : Values)
		{
			const double AdjustedValue = FMath::RoundToZero(Value / Tolerance) * Tolerance;
			const int32* Count = Map.Find(AdjustedValue);
			const int32 UpdatedCount = Count ? *Count + 1 : 1;
			Map.Add(Value, UpdatedCount);

			if (LastCount < UpdatedCount)
			{
				LastCount = UpdatedCount;
				Mode = AdjustedValue;
			}
			else if (LastCount == UpdatedCount)
			{
				if (bHighest) { Mode = FMath::Max(Mode, AdjustedValue); }
				else { Mode = FMath::Min(Mode, AdjustedValue); }
			}
		}

		return Mode;
	}

	FVector SafeLinePlaneIntersection(const FVector& Pt1, const FVector& Pt2, const FVector& PlaneOrigin, const FVector& PlaneNormal, bool& bIntersect)
	{
		if (FMath::IsNearlyZero(FVector::DotProduct((Pt1 - Pt2).GetSafeNormal(), PlaneNormal)))
		{
			bIntersect = false;
			return FVector::ZeroVector;
		}

		bIntersect = true;
		return FMath::LinePlaneIntersection(Pt1, Pt2, PlaneOrigin, PlaneNormal);
	}

	bool SphereOverlap(const FSphere& S1, const FSphere& S2, double& OutOverlap)
	{
		OutOverlap = (S1.W + S2.W) - FVector::Dist(S1.Center, S2.Center);
		return OutOverlap > 0;
	}

	bool SphereOverlap(const FBoxSphereBounds& S1, const FBoxSphereBounds& S2, double& OutOverlap)
	{
		return SphereOverlap(S1.GetSphere(), S2.GetSphere(), OutOverlap);
	}

	FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward: return GetDirection<EPCGExAxis::Forward>(Quat);
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
		default:
		case EPCGExAxis::Forward:
			return PCGEX_AXIS_X;
		case EPCGExAxis::Backward:
			return PCGEX_AXIS_X_N;
		case EPCGExAxis::Right:
			return PCGEX_AXIS_Y;
		case EPCGExAxis::Left:
			return PCGEX_AXIS_Y_N;
		case EPCGExAxis::Up:
			return PCGEX_AXIS_Z;
		case EPCGExAxis::Down:
			return PCGEX_AXIS_Z_N;
		}
	}

	FTransform GetIdentity(const EPCGExAxisOrder Order)
	{
		switch (Order)
		{
		default:
		case EPCGExAxisOrder::XYZ:
			return FTransform(FMatrix(PCGEX_AXIS_X, PCGEX_AXIS_Y, PCGEX_AXIS_Z, FVector::Zero()));
		case EPCGExAxisOrder::YZX:
			return FTransform(FMatrix(PCGEX_AXIS_Y, PCGEX_AXIS_Z, PCGEX_AXIS_X, FVector::Zero()));
		case EPCGExAxisOrder::ZXY:
			return FTransform(FMatrix(PCGEX_AXIS_Z, PCGEX_AXIS_X, PCGEX_AXIS_Y, FVector::Zero()));
		case EPCGExAxisOrder::YXZ:
			return FTransform(FMatrix(PCGEX_AXIS_Y, PCGEX_AXIS_X, PCGEX_AXIS_Z, FVector::Zero()));
		case EPCGExAxisOrder::ZYX:
			return FTransform(FMatrix(PCGEX_AXIS_Z, PCGEX_AXIS_Y, PCGEX_AXIS_X, FVector::Zero()));
		case EPCGExAxisOrder::XZY:
			return FTransform(FMatrix(PCGEX_AXIS_X, PCGEX_AXIS_Z, PCGEX_AXIS_Y, FVector::Zero()));
		}
	}

	void Swizzle(FVector& Vector, const EPCGExAxisOrder Order)
	{
		int32 A;
		int32 B;
		int32 C;
		PCGEx::GetAxisOrder(Order, A, B, C);
		FVector Temp = Vector;
		Vector[0] = Temp[A];
		Vector[1] = Temp[B];
		Vector[2] = Temp[C];
	}

	void Swizzle(FVector& Vector, const int32(& Order)[3])
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
		default:
		case EPCGExAxis::Forward:
			return FRotationMatrix::MakeFromX(InForward * -1).ToQuat();
		case EPCGExAxis::Backward:
			return FRotationMatrix::MakeFromX(InForward).ToQuat();
		case EPCGExAxis::Right:
			return FRotationMatrix::MakeFromY(InForward * -1).ToQuat();
		case EPCGExAxis::Left:
			return FRotationMatrix::MakeFromY(InForward).ToQuat();
		case EPCGExAxis::Up:
			return FRotationMatrix::MakeFromZ(InForward * -1).ToQuat();
		case EPCGExAxis::Down:
			return FRotationMatrix::MakeFromZ(InForward).ToQuat();
		}
	}

	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward:
			return FRotationMatrix::MakeFromXZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Backward:
			return FRotationMatrix::MakeFromXZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Right:
			return FRotationMatrix::MakeFromYZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Left:
			return FRotationMatrix::MakeFromYZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Up:
			return FRotationMatrix::MakeFromZY(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Down:
			return FRotationMatrix::MakeFromZY(InForward, InUp).ToQuat();
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
		case EPCGExAxisAlign::Forward:
			return FTransform(FRotationMatrix::MakeFromXZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Backward:
			return FTransform(FRotationMatrix::MakeFromXZ(LookAt, LookUp));
		case EPCGExAxisAlign::Right:
			return FTransform(FRotationMatrix::MakeFromYZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Left:
			return FTransform(FRotationMatrix::MakeFromYZ(LookAt, LookUp));
		case EPCGExAxisAlign::Up:
			return FTransform(FRotationMatrix::MakeFromZY(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Down:
			return FTransform(FRotationMatrix::MakeFromZY(LookAt, LookUp));
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

	void CheckConvex(const FVector& A, const FVector& B, const FVector& C, bool& bIsConvex, int32& OutSign, const FVector& UpVector)
	{
		if (!bIsConvex) { return; }

		if (A == C)
		{
			bIsConvex = false;
			return;
		}

		const double DP = FVector::DotProduct(FVector::CrossProduct((A - B), (C - A)), UpVector);
		const int32 CurrentSign = (DP > 0.0f) ? 1 : (DP < 0.0f) ? -1 : 0;

		if (CurrentSign != 0)
		{
			if (OutSign == 0) { OutSign = CurrentSign; }
			else if (OutSign != CurrentSign) { bIsConvex = false; }
		}
	}

	FBox ScaledBox(const FBox& InBox, const FVector& InScale)
	{
		const FVector Extents = InBox.GetExtent() * InScale;
		return FBox(-Extents, Extents);
	}

	bool IsDirectionWithinTolerance(const FVector& A, const FVector& B, const FRotator& Limits)
	{
		const FRotator RA = A.Rotation();
		const FRotator RB = B.Rotation();

		return
			FMath::Abs(FRotator::NormalizeAxis(RA.Yaw - RB.Yaw)) <= Limits.Yaw &&
			FMath::Abs(FRotator::NormalizeAxis(RA.Pitch - RB.Pitch)) <= Limits.Pitch &&
			FMath::Abs(FRotator::NormalizeAxis(RA.Roll - RB.Roll)) <= Limits.Roll;
	}

	double GetArcLength(const double R, const double StartAngleRadians, const double EndAngleRadians)
	{
		return R * FMath::Abs(FMath::Fmod(EndAngleRadians, TWO_PI) - FMath::Fmod(StartAngleRadians, TWO_PI));
	}

	double GetPerpendicularDistance(const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector AB = B - A;
		return FVector::Dist(C, A + FMath::Clamp(FVector::DotProduct((C - A), AB) / AB.SizeSquared(), 0, 1) * AB);
	}
}

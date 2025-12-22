// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExMath.h"
#include "PCGExCoreMacros.h"

namespace PCGExMath
{
	double TruncateDbl(const double Value, const EPCGExTruncateMode Mode)
	{
		switch (Mode)
		{
		case EPCGExTruncateMode::Round: return FMath::RoundToInt(Value);
		case EPCGExTruncateMode::Ceil: return FMath::CeilToDouble(Value);
		case EPCGExTruncateMode::Floor: return FMath::FloorToDouble(Value);
		default: case EPCGExTruncateMode::None: return Value;
		}
	}

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
		: A(InA), B(InB), Direction((B - A).GetSafeNormal()), Bounds(PCGEX_BOX_TOLERANCE_INLINE(A, B, Expansion))
	{
	}

	bool FSegment::FindIntersection(const FVector& A2, const FVector& B2, const double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const uint8 Strictness) const
	{
		FMath::SegmentDistToSegment(A, B, A2, B2, OutSelf, OutOther);

		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::MainA)) && A == OutSelf) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::MainB)) && B == OutSelf) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::OtherA)) && A2 == OutOther) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::OtherB)) && B2 == OutOther) { return false; }

		if (FVector::DistSquared(OutSelf, OutOther) >= SquaredTolerance) { return false; }
		return true;
	}

	bool FSegment::FindIntersection(const FSegment& S, const double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const uint8 Strictness) const
	{
		FMath::SegmentDistToSegment(A, B, S.A, S.B, OutSelf, OutOther);

		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::MainA)) && A == OutSelf) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::MainB)) && B == OutSelf) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::OtherA)) && S.A == OutOther) { return false; }
		if ((Strictness & static_cast<uint8>(EPCGExIntersectionStrictness::OtherB)) && S.B == OutOther) { return false; }

		if (FVector::DistSquared(OutSelf, OutOther) >= SquaredTolerance) { return false; }
		return true;
	}

	double ConvertStringToDouble(const FString& StringToConvert)
	{
		const TCHAR* CharArray = *StringToConvert;
		const double Result = FCString::Atod(CharArray);
		return FMath::IsNaN(Result) ? 0 : Result;
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

		return FMath::Abs(FRotator::NormalizeAxis(RA.Yaw - RB.Yaw)) <= Limits.Yaw && FMath::Abs(FRotator::NormalizeAxis(RA.Pitch - RB.Pitch)) <= Limits.Pitch && FMath::Abs(FRotator::NormalizeAxis(RA.Roll - RB.Roll)) <= Limits.Roll;
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

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExSampling.generated.h"

#define PCGEX_OUT_ATTRIBUTE(_NAME, _TYPE)\
bool bWrite##_NAME = false;\
FName OutName##_NAME = NAME_None;\
FPCGMetadataAttribute<_TYPE>* OutAttribute##_NAME = nullptr;

#define PCGEX_FORWARD_OUT_ATTRIBUTE(_NAME)\
Context->bWrite##_NAME = Settings->bWrite##_NAME;\
Context->OutName##_NAME = Settings->_NAME;

#define PCGEX_CHECK_OUT_ATTRIBUTE_NAME(_NAME)\
if(Context->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Context->OutName##_NAME))\
{ PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidName", "Invalid output attribute name " #_NAME ));\
Context->bWrite##_NAME = false; }

#define PCGEX_SET_OUT_ATTRIBUTE(_NAME, _KEY, _VALUE)\
if (Context->OutAttribute##_NAME) { Context->OutAttribute##_NAME->SetValue(_KEY, _VALUE); }

#define PCGEX_INIT_ATTRIBUTE_OUT(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::TryGetAttribute<_TYPE>(PointIO->Out, Context->OutName##_NAME, Context->bWrite##_NAME);
#define PCGEX_INIT_ATTRIBUTE_IN(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::TryGetAttribute<_TYPE>(PointIO->In, Context->OutName##_NAME, Context->bWrite##_NAME);

UENUM(BlueprintType)
enum class EPCGExSampleMethod : uint8
{
	WithinRange UMETA(DisplayName = "Within Range", ToolTip="Use RangeMax = 0 to include all targets"),
	ClosestTarget UMETA(DisplayName = "Closest Target", ToolTip="TBD"),
	FarthestTarget UMETA(DisplayName = "Farthest Target", ToolTip="TBD"),
	TargetsExtents UMETA(DisplayName = "Targets Extents", ToolTip="TBD"),
};

UENUM(BlueprintType)
enum class EPCGExWeightMethod : uint8
{
	FullRange UMETA(DisplayName = "Full Range", ToolTip="Weight is sampled using the normalized distance over the full min/max range."),
	EffectiveRange UMETA(DisplayName = "Effective Range", ToolTip="Weight is sampled using the normalized distance over the min/max of sampled points."),
};

UENUM(BlueprintType)
enum class EPCGExAngleRange : uint8
{
	URadians UMETA(DisplayName = "Radians (0..+PI)", ToolTip="0..+PI"),
	PIRadians UMETA(DisplayName = "Radians (-PI..+PI)", ToolTip="-PI..+PI"),
	TAURadians UMETA(DisplayName = "Radians (0..+TAU)", ToolTip="0..TAU"),
	UDegrees UMETA(DisplayName = "Degrees (0..+180)", ToolTip="0..+180"),
	PIDegrees UMETA(DisplayName = "Degrees (-180..+180)", ToolTip="-180..+180"),
	TAUDegrees UMETA(DisplayName = "Degrees (0..+360)", ToolTip="0..+360"),
};

namespace PCGExSampling
{
	static void GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B, double& OutAngle)
	{
		const FVector N1 = A.GetSafeNormal();
		const FVector N2 = B.GetSafeNormal();

		const double MainDot = N1.Dot(N2);

		switch (Mode)
		{
		case EPCGExAngleRange::URadians: // 0 .. 3.14
			OutAngle = FMath::Acos(MainDot);
			break;
		case EPCGExAngleRange::PIRadians: // -3.14 .. 3.14
			OutAngle = FMath::Acos(MainDot) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAURadians: // 0 .. 6.28
			if (FVector::CrossProduct(N1, N2).Z < 0)
			{
				OutAngle = (PI * 2) - FMath::Atan2(FVector::CrossProduct(N1, N2).Size(), MainDot);
			}
			else
			{
				OutAngle = FMath::Atan2(FVector::CrossProduct(N1, N2).Size(), MainDot);
			}
			break;
		case EPCGExAngleRange::UDegrees: // 0 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot));
			break;
		case EPCGExAngleRange::PIDegrees: // -180 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot)) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAUDegrees: // 0 .. 360
			if (FVector::CrossProduct(N1, N2).Z < 0)
			{
				OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(N1, N2).Size(), MainDot));
			}
			else
			{
				OutAngle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(N1, N2).Size(), MainDot));
			}
			break;
		default: ;
		}
	}
}

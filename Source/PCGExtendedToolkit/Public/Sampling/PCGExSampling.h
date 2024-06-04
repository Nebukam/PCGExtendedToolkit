// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMT.h"
#include "PCGExSampling.generated.h"

#define PCGEX_OUTPUT_DECL(_NAME, _TYPE) PCGEx::TFAttributeWriter<_TYPE>* _NAME##Writer = nullptr;
#define PCGEX_OUTPUT_FWD(_NAME, _TYPE) Context->_NAME##Writer = Settings->bWrite##_NAME ? new PCGEx::TFAttributeWriter<_TYPE>(Settings->_NAME##AttributeName) : nullptr;

#define PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_SOFT(_NAME)\
if(Context->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ Context->bWrite##_NAME = false; PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME ));}

#define PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER(_NAME)\
if(Settings->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME ));}

#define PCGEX_OUTPUT_VALIDATE_NAME(_NAME, _TYPE)\
if(Settings->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME ));\
PCGEX_DELETE(Context->_NAME##Writer)}

#define PCGEX_OUTPUT_VALUE(_NAME, _INDEX, _VALUE) if(Context->_NAME##Writer){(*Context->_NAME##Writer)[_INDEX] = _VALUE; }
#define PCGEX_OUTPUT_WRITE(_NAME, _TYPE) if(Context->_NAME##Writer){Context->_NAME##Writer->Write();}
#define PCGEX_OUTPUT_ACCESSOR_INIT(_NAME, _TYPE) if(Context->_NAME##Writer){Context->_NAME##Writer->BindAndGet(PointIO);}
#define PCGEX_OUTPUT_ACCESSOR_INIT_PTR(_NAME, _TYPE) if(Context->_NAME##Writer){Context->_NAME##Writer->BindAndGet(*PointIO);}
#define PCGEX_OUTPUT_DELETE(_NAME, _TYPE) PCGEX_DELETE(_NAME##Writer)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Sample Method"))
enum class EPCGExSampleMethod : uint8
{
	WithinRange UMETA(DisplayName = "All (Within range)", ToolTip="Use RangeMax = 0 to include all targets"),
	ClosestTarget UMETA(DisplayName = "Closest Target", ToolTip="Picks & process the closest target only"),
	FarthestTarget UMETA(DisplayName = "Farthest Target", ToolTip="Picks & process the farthest target only"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Sample Source"))
enum class EPCGExSampleSource : uint8
{
	Source UMETA(DisplayName = "Source", ToolTip="Read value on source"),
	Target UMETA(DisplayName = "Target", ToolTip="Read value on target"),
	Constant UMETA(DisplayName = "Constant", ToolTip="Read constant"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Angle Range"))
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
	const FName SourceIgnoreActorsLabel = TEXT("InIgnoreActors");
	const FName OutputSampledActorsLabel = TEXT("OutSampledActors");

	FORCEINLINE static double GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B)
	{
		double OutAngle = 0;

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

		return OutAngle;
	}
}

class PCGEXTENDEDTOOLKIT_API FPCGExPCGExCollisionTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPCGExCollisionTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}
};

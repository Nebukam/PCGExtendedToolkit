// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "Data/PCGExData.h"

#include "PCGExSampling.generated.h"

// Declaration & use pair, boolean will be set by name validation
#define PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE) bool bWrite##_NAME = false;
#define PCGEX_OUTPUT_DECL(_NAME, _TYPE) PCGEx:: TAttributeWriter<_TYPE>* _NAME##Writer = nullptr;
#define PCGEX_OUTPUT_DECL_AND_TOGGLE(_NAME, _TYPE) PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE) PCGEX_OUTPUT_DECL(_NAME, _TYPE)

// Create writer in context
#define PCGEX_OUTPUT_FWD_C(_NAME, _TYPE) if(Context->bWrite##_NAME){ Context->_NAME##Writer =  new PCGEx:: TAttributeWriter<_TYPE>(Settings->_NAME##AttributeName); }

// Create writer in root
#define PCGEX_OUTPUT_FWD(_NAME, _TYPE) if(TypedContext->bWrite##_NAME){ _NAME##Writer = new PCGEx:: TAttributeWriter<_TYPE>(Settings->_NAME##AttributeName); }

// Simply validate name from settings
#define PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_C(_NAME, _TYPE)\
if(Settings->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME )); }

// Simply validate name from settings
#define PCGEX_OUTPUT_VALIDATE_NAME(_NAME, _TYPE)\
Context->bWrite##_NAME = Settings->bWrite##_NAME; \
if(Context->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME )); Context->bWrite##_NAME = false; }

#define PCGEX_OUTPUT_VALUE(_NAME, _INDEX, _VALUE) if(_NAME##Writer){_NAME##Writer->Values[_INDEX] = _VALUE; }
#define PCGEX_OUTPUT_ACCESSOR_INIT(_NAME, _TYPE) if(_NAME##Writer){_NAME##Writer->BindAndSetNumUninitialized(OutputIO);}
#define PCGEX_OUTPUT_INIT(_NAME, _TYPE) if(TypedContext->bWrite##_NAME){ _NAME##Writer = OutputFacade->GetWriter<_TYPE>(Settings->_NAME##AttributeName, true); }
#define PCGEX_OUTPUT_INIT_DEFAULT(_NAME, _TYPE) if(TypedContext->bWrite##_NAME){ _NAME##Writer = OutputFacade->GetWriter<_TYPE>(Settings->_NAME##AttributeName, false); }

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Surface Source"))
enum class EPCGExSurfaceSource : uint8
{
	All UMETA(DisplayName = "Any surface", ToolTip="Any surface within range will be tested"),
	ActorReferences UMETA(DisplayName = "Actor Reference", ToolTip="Only a list of actor surfaces will be included."),
};


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
	const FName SourceActorReferencesLabel = TEXT("ActorReferences");
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

	static bool GetIncludedActors(
		const FPCGContext* InContext,
		const PCGExData::FFacade* InFacade,
		const FName ActorReferenceName,
		TMap<AActor*, int32>& OutActorSet)
	{
		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.SetAttributeName(ActorReferenceName);

		PCGEx::FLocalToStringGetter* PathGetter = new PCGEx::FLocalToStringGetter();
		PathGetter->Capture(Selector);
		if (!PathGetter->SoftGrab(InFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Actor reference attribute does not exist."));
			PCGEX_DELETE(PathGetter)
			return false;
		}

		const TArray<FPCGPoint>& TargetPoints = InFacade->GetIn()->GetPoints();
		for (int i = 0; i < TargetPoints.Num(); i++)
		{
			FSoftObjectPath Path = PathGetter->SoftGet(i, TargetPoints[i], TEXT(""));

			if (!Path.IsValid()) { continue; }

			if (UObject* FoundObject = FindObject<AActor>(nullptr, *Path.ToString()))
			{
				if (AActor* TargetActor = Cast<AActor>(FoundObject)) { OutActorSet.FindOrAdd(TargetActor, i); }
			}
		}

		PCGEX_DELETE(PathGetter)
		return true;
	}
}

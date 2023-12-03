// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingWeight.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);
#define PCGEX_SET(_BODY) PrimaryAttribute->SetValue(InPrimaryOutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET)\
void UPCGExDataBlendingWeight##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const { PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };

PCGEX_SAO_CLASS(bool, Boolean, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(int32, Integer32, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(int64, Integer64, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(float, Float, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(double, Double, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FVector2D, Vector2, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FVector, Vector, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FVector4, Vector4, FMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FQuat, Quaternion, PCGExMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FRotator, Rotator, PCGExMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::Lerp(A, B, Alpha))
PCGEX_SAO_CLASS(FString, String, Alpha > 0.5 ? B : A)
PCGEX_SAO_CLASS(FName, Name, Alpha > 0.5 ? B : A)

#undef PCGEX_DECL
#undef PCGEX_SET
#undef PCGEX_SAO_CLASS

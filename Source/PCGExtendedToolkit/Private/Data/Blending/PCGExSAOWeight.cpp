// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExSAOWeight.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetValue(OperandAKey); const _TYPE B = GetValue(OperandBKey);
#define PCGEX_SET(_BODY) Attribute->SetValue(OutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET)\
void UPCGExSAOWeight##_NAME::DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha){ PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };

PCGEX_SAO_CLASS(bool, Boolean, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(int32, Integer32, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(int64, Integer64, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(float, Float, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(double, Double, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FVector2D, Vector2, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FVector, Vector, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FVector4, Vector4, FMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FQuat, Quaternion, PCGExMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FRotator, Rotator, PCGExMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::Lerp(B, A,Alpha))
PCGEX_SAO_CLASS(FString, String, Alpha > 0.5 ? A : B)
PCGEX_SAO_CLASS(FName, Name, Alpha > 0.5 ? A : B)

#undef PCGEX_DECL
#undef PCGEX_SET
#undef PCGEX_SAO_CLASS

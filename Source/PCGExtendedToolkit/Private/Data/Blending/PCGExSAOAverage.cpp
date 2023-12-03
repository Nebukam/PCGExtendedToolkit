// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExSAOAverage.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetValue(OperandAKey); const _TYPE B = GetValue(OperandBKey);
#define PCGEX_SET(_BODY) Attribute->SetValue(OutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET, _SET_FINAL)\
bool UPCGExSAOAverage##_NAME::UsePreparation() const { return true; }\
bool UPCGExSAOAverage##_NAME::UseFinalize() const { return true; }\
void UPCGExSAOAverage##_NAME::PrepareOperation(const PCGMetadataEntryKey OutputKey) const { ResetToDefault(OutputKey); }\
void UPCGExSAOAverage##_NAME::DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha)const { PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };\
void UPCGExSAOAverage##_NAME::FinalizeOperation(const PCGMetadataEntryKey OutputKey, double Alpha) const { const _TYPE Val = GetValue(OutputKey); PCGEX_SET(_SET_FINAL) };

/*
PCGEX_SAO_CLASS(bool, Boolean, A + B, Val)
PCGEX_SAO_CLASS(int32, Integer32, A +B, Val / Alpha)
PCGEX_SAO_CLASS(int64, Integer64, A +B, Val / Alpha)
PCGEX_SAO_CLASS(double, Double, A +B, Val / Alpha)
PCGEX_SAO_CLASS(FVector2D, Vector2, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FVector, Vector, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FVector4, Vector4, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FQuat, Quaternion, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FRotator, Rotator, A + B, PCGExMath::CWDivide(Val, Alpha))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::Add(A, B), PCGExMath::CWDivide(Val, Alpha))
PCGEX_SAO_CLASS(FString, String, A < B ? A : B, Val)
PCGEX_SAO_CLASS(FName, Name, A.ToString() < B.ToString() ? B : B, Val)
*/

#pragma region GENERATED

PCGEX_SAO_CLASS(bool, Boolean, A + B, Val)
PCGEX_SAO_CLASS(int32, Integer32, A + B, Val / Alpha)
PCGEX_SAO_CLASS(int64, Integer64, A + B, Val / Alpha)
PCGEX_SAO_CLASS(float, Float, A + B, Val / Alpha)
PCGEX_SAO_CLASS(double, Double, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FVector2D, Vector2, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FVector, Vector, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FVector4, Vector4, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FQuat, Quaternion, A + B, Val / Alpha)
PCGEX_SAO_CLASS(FRotator, Rotator, A + B, PCGExMath::CWDivide(Val, Alpha))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::Add(A, B), PCGExMath::CWDivide(Val, Alpha))
PCGEX_SAO_CLASS(FString, String, A < B ? A : B, Val)
PCGEX_SAO_CLASS(FName, Name, A.ToString() < B.ToString() ? B : B, Val)

#pragma endregion

#undef PCGEX_DECL
#undef PCGEX_SET
#undef PCGEX_SAO_CLASS

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingAverage.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);
#define PCGEX_SET(_BODY) PrimaryAttribute->SetValue(InPrimaryOutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET, _SET_FINAL)\
bool UPCGExDataBlendingAverage##_NAME::GetRequiresPreparation() const { return true; }\
bool UPCGExDataBlendingAverage##_NAME::GetRequiresFinalization() const { return true; }\
void UPCGExDataBlendingAverage##_NAME::PrepareOperation(const PCGMetadataEntryKey InPrimaryOutputKey) const { ResetToDefault(InPrimaryOutputKey); }\
void UPCGExDataBlendingAverage##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha)const { PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };\
void UPCGExDataBlendingAverage##_NAME::FinalizeOperation(const PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const { const _TYPE Val = GetPrimaryValue(InPrimaryOutputKey); PCGEX_SET(_SET_FINAL) };

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

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingMax.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);
#define PCGEX_SET(_BODY) PrimaryAttribute->SetValue(InPrimaryOutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET)\
void UPCGExDataBlendingMax##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const { PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };

PCGEX_SAO_CLASS(bool, Boolean, FMath::Max(A, B))
PCGEX_SAO_CLASS(int32, Integer32, FMath::Max(A, B))
PCGEX_SAO_CLASS(int64, Integer64, FMath::Max(A, B))
PCGEX_SAO_CLASS(float, Float, FMath::Max(A, B))
PCGEX_SAO_CLASS(double, Double, FMath::Max(A, B))
PCGEX_SAO_CLASS(FVector2D, Vector2, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FVector, Vector, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FVector4, Vector4, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FQuat, Quaternion, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FRotator, Rotator, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::CWMax(A, B))
PCGEX_SAO_CLASS(FString, String, FMath::Max(A, B))
PCGEX_SAO_CLASS(FName, Name, A.ToString() < B.ToString() ? A : B)

#undef PCGEX_DECL
#undef PCGEX_SET
#undef PCGEX_SAO_CLASS

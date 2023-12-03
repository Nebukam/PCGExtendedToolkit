// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingMin.h"

#include "PCGExMath.h"

#define PCGEX_DECL(_TYPE) const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);
#define PCGEX_SET(_BODY) PrimaryAttribute->SetValue(InPrimaryOutputKey, _BODY);
#define PCGEX_SAO_CLASS(_TYPE, _NAME, _SET)\
void UPCGExDataBlendingMin##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const  PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const { PCGEX_DECL(_TYPE) PCGEX_SET(_SET) };

PCGEX_SAO_CLASS(bool, Boolean, FMath::Min(A, B))
PCGEX_SAO_CLASS(int32, Integer32, FMath::Min(A, B))
PCGEX_SAO_CLASS(int64, Integer64, FMath::Min(A, B))
PCGEX_SAO_CLASS(float, Float, FMath::Min(A, B))
PCGEX_SAO_CLASS(double, Double, FMath::Min(A, B))
PCGEX_SAO_CLASS(FVector2D, Vector2, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FVector, Vector, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FVector4, Vector4, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FQuat, Quaternion, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FRotator, Rotator, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FTransform, Transform, PCGExMath::CWMin(A, B))
PCGEX_SAO_CLASS(FString, String, FMath::Min(A, B))
PCGEX_SAO_CLASS(FName, Name, A.ToString() < B.ToString() ? A : B)

#undef PCGEX_DECL
#undef PCGEX_SET
#undef PCGEX_SAO_CLASS

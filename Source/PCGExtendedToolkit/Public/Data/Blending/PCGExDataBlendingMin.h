// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlendingOperation.h"
#include "PCGExDataBlendingMin.generated.h"

#define PCGEX_SAO_DECL(_TYPE, _NAME)\
public:\
virtual void DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha = 0) const override;
#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
UCLASS(Blueprintable, EditInlineNew, HideDropdown)\
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMin##_NAME : public UPCGExBlend##_NAME##Base{\
GENERATED_BODY()\
PCGEX_SAO_DECL(_TYPE, _NAME)};

// Note: We're not using template for a slight performance boost

/*
PCGEX_SAO_CLASS(bool, Boolean)
PCGEX_SAO_CLASS(int32, Integer32)
PCGEX_SAO_CLASS(int64, Integer64)
PCGEX_SAO_CLASS(float, Float)
PCGEX_SAO_CLASS(double, Double)
PCGEX_SAO_CLASS(FVector2D, Vector2)
PCGEX_SAO_CLASS(FVector, Vector)
PCGEX_SAO_CLASS(FVector4, Vector4)
PCGEX_SAO_CLASS(FQuat, Quaternion)
PCGEX_SAO_CLASS(FRotator, Rotator)
PCGEX_SAO_CLASS(FTransform, Transform)
PCGEX_SAO_CLASS(FString, String)
PCGEX_SAO_CLASS(FName, Name)
*/

#pragma region GENERATED
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinBoolean : public UPCGExBlendBooleanBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(bool, Boolean)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinInteger32 : public UPCGExBlendInteger32Base
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(int32, Integer32)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinInteger64 : public UPCGExBlendInteger64Base
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(int64, Integer64)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinFloat : public UPCGExBlendFloatBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(float, Float)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinDouble : public UPCGExBlendDoubleBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(double, Double)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinVector2 : public UPCGExBlendVector2Base
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FVector2D, Vector2)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinVector : public UPCGExBlendVectorBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FVector, Vector)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinVector4 : public UPCGExBlendVector4Base
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FVector4, Vector4)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinQuaternion : public UPCGExBlendQuaternionBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FQuat, Quaternion)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinRotator : public UPCGExBlendRotatorBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FRotator, Rotator)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinTransform : public UPCGExBlendTransformBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FTransform, Transform)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinString : public UPCGExBlendStringBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FString, String)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingMinName : public UPCGExBlendNameBase
{
	GENERATED_BODY()
	PCGEX_SAO_DECL(FName, Name)
};
#pragma endregion

#undef PCGEX_SAO_DECL
#undef PCGEX_SAO_CLASS

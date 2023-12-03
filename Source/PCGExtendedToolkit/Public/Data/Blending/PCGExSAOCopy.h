// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMetadataOperation.h"
#include "PCGExSAOCopy.generated.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
UCLASS(Blueprintable, EditInlineNew, HideDropdown)\
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopy##_NAME : public UPCGExBlend##_NAME##Base{\
GENERATED_BODY()\
public:\
virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;\
};

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
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyBoolean : public UPCGExBlendBooleanBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyInteger32 : public UPCGExBlendInteger32Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyInteger64 : public UPCGExBlendInteger64Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyFloat : public UPCGExBlendFloatBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyDouble : public UPCGExBlendDoubleBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyVector2 : public UPCGExBlendVector2Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyVector : public UPCGExBlendVectorBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyVector4 : public UPCGExBlendVector4Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyQuaternion : public UPCGExBlendQuaternionBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyRotator : public UPCGExBlendRotatorBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyTransform : public UPCGExBlendTransformBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyString : public UPCGExBlendStringBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOCopyName : public UPCGExBlendNameBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) const override;
};

#pragma endregion

#undef PCGEX_SAO_CLASS

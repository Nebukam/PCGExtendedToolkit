// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSingleAttributeOperation.h"
#include "PCGExSAOMax.generated.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
UCLASS(Blueprintable, EditInlineNew, HideDropdown)\
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMax##_NAME : public UPCGExBlend##_NAME##Base{\
GENERATED_BODY()\
public:\
virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) override;\
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
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxBoolean : public UPCGExBlendBooleanBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxInteger32 : public UPCGExBlendInteger32Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxInteger64 : public UPCGExBlendInteger64Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxFloat : public UPCGExBlendFloatBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxDouble : public UPCGExBlendDoubleBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxVector2 : public UPCGExBlendVector2Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxVector : public UPCGExBlendVectorBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxVector4 : public UPCGExBlendVector4Base
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxQuaternion : public UPCGExBlendQuaternionBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxRotator : public UPCGExBlendRotatorBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxTransform : public UPCGExBlendTransformBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxString : public UPCGExBlendStringBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOMaxName : public UPCGExBlendNameBase
{
	GENERATED_BODY()

public:
	virtual void DoOperation(PCGMetadataEntryKey OperandAKey, PCGMetadataEntryKey OperandBKey, PCGMetadataEntryKey OutputKey, double Alpha = 0) override;
};

#pragma endregion

#undef PCGEX_SAO_CLASS

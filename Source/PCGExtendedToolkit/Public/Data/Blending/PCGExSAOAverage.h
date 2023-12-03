﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMetadataOperation.h"
#include "PCGExSAOAverage.generated.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
UCLASS(Blueprintable, EditInlineNew, HideDropdown)\
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverage##_NAME : public UPCGExBlend##_NAME##Base{\
GENERATED_BODY()\
public:\
virtual bool UsePreparation() const override;\
virtual bool UseFinalize() const override;\
virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;\
virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;\
virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const  override;\
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
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageBoolean : public UPCGExBlendBooleanBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageInteger32 : public UPCGExBlendInteger32Base
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageInteger64 : public UPCGExBlendInteger64Base
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageFloat : public UPCGExBlendFloatBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageDouble : public UPCGExBlendDoubleBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageVector2 : public UPCGExBlendVector2Base
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageVector : public UPCGExBlendVectorBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageVector4 : public UPCGExBlendVector4Base
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageQuaternion : public UPCGExBlendQuaternionBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageRotator : public UPCGExBlendRotatorBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageTransform : public UPCGExBlendTransformBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageString : public UPCGExBlendStringBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSAOAverageName : public UPCGExBlendNameBase
{
	GENERATED_BODY()

public:
	virtual bool UsePreparation() const override;
	virtual bool UseFinalize() const override;
	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const override;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const override;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, const double Alpha) const override;
};

#pragma endregion

#undef PCGEX_SAO_CLASS
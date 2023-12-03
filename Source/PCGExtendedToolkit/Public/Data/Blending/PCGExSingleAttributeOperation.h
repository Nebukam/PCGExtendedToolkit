// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExInstruction.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "PCGExSingleAttributeOperation.generated.h"

#define PCGEX_FORCE_INLINE_GETTER_DECL(_TYPE)\
FPCGMetadataAttribute<_TYPE>* Attribute;\
inline virtual _TYPE GetValue(PCGMetadataAttributeKey Key);
#define PCGEX_FORCE_INLINE_GETTER_IMPL(_TYPE) __forceinline virtual _TYPE GetValue(PCGMetadataAttributeKey Key);

#define PCGEX_SAO_CLASS(_TYPE, _NAME) \
UCLASS(Blueprintable, EditInlineNew, HideDropdown) \
class PCGEXTENDEDTOOLKIT_API UPCGExBlend##_NAME##Base : public UPCGExSingleAttributeOperation{\
GENERATED_BODY()\
public:	\
virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;\
virtual void PrepareForData(UPCGPointData* InData) override;\
protected:	PCGEX_FORCE_INLINE_GETTER_DECL(_TYPE) };

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSingleAttributeOperation : public UPCGExInstruction
{
	GENERATED_BODY()

public:
	void SetAttributeName(FName InName) { AttributeName = InName; }
	FName GetAttributeName() const { return AttributeName; }

	virtual void PrepareForData(UPCGPointData* InData);

	virtual bool UseFinalize();

	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0);
	virtual void FinalizeOperation(PCGMetadataEntryKey OutputKey, double Alpha);
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey);

protected:
	FName AttributeName = NAME_None;
	FPCGMetadataAttributeBase* BaseAttribute;
};

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
class PCGEXTENDEDTOOLKIT_API UPCGExBlendBooleanBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(bool)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger32Base : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(int32)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger64Base : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(int64)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendFloatBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(float)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendDoubleBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(double)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector2Base : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector2D)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVectorBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector4Base : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector4)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendQuaternionBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FQuat)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendRotatorBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FRotator)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendTransformBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FTransform)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendStringBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FString)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendNameBase : public UPCGExSingleAttributeOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) override;
	virtual void PrepareForData(UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FName)
};

#pragma endregion

#undef PCGEX_FORCE_INLINE_GETTER_DECL
#undef PCGEX_FORCE_INLINE_GETTER_IMPL
#undef PCGEX_SAO_CLASS

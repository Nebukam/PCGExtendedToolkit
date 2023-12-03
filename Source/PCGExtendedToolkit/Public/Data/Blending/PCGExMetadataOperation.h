// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExInstruction.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "PCGExMetadataOperation.generated.h"

#define PCGEX_FORCE_INLINE_GETTER_DECL(_TYPE)\
FPCGMetadataAttribute<_TYPE>* Attribute;\
inline virtual _TYPE GetValue(const PCGMetadataAttributeKey& Key) const;

#define PCGEX_SAO_CLASS(_TYPE, _NAME) \
UCLASS(Blueprintable, EditInlineNew, HideDropdown) \
class PCGEXTENDEDTOOLKIT_API UPCGExBlend##_NAME##Base : public UPCGExMetadataOperation{\
GENERATED_BODY()\
public:	\
virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;\
virtual void PrepareForData(const UPCGPointData* InData) const override;\
protected:	PCGEX_FORCE_INLINE_GETTER_DECL(_TYPE) };

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExMetadataOperation : public UPCGExInstruction
{
	GENERATED_BODY()

public:
	void SetAttributeName(FName InName) { AttributeName = InName; }
	FName GetAttributeName() const { return AttributeName; }

	virtual void PrepareForData(const UPCGPointData* InData);

	virtual bool UsePreparation() const;
	virtual bool UseFinalize() const;

	virtual void PrepareOperation(const PCGMetadataEntryKey OutputKey) const;
	virtual void DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const;
	virtual void FinalizeOperation(const PCGMetadataEntryKey OutputKey, double Alpha) const;
	virtual void ResetToDefault(const PCGMetadataEntryKey OutputKey) const;

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
class PCGEXTENDEDTOOLKIT_API UPCGExBlendBooleanBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(bool)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger32Base : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(int32)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger64Base : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(int64)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendFloatBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(float)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendDoubleBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(double)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector2Base : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector2D)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVectorBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector4Base : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FVector4)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendQuaternionBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FQuat)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendRotatorBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FRotator)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendTransformBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FTransform)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendStringBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FString)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendNameBase : public UPCGExMetadataOperation
{
	GENERATED_BODY()

public:
	virtual void ResetToDefault(PCGMetadataEntryKey OutputKey) const override;
	virtual void PrepareForData(const UPCGPointData* InData) override;

protected:
	PCGEX_FORCE_INLINE_GETTER_DECL(FName)
};

#pragma endregion

#undef PCGEX_FORCE_INLINE_GETTER_DECL
#undef PCGEX_SAO_CLASS

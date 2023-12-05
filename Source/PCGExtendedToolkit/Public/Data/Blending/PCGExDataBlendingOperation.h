// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExInstruction.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "PCGExDataBlendingOperation.generated.h"

#define PCGEX_SAO_BODY(_TYPE, _NAME)\
public:	\
virtual void ResetToDefault(PCGMetadataEntryKey InPrimaryOutputKey) const override;\
protected:	\
virtual void StrongTypeAttributes() override;\
FPCGMetadataAttribute<_TYPE>* PrimaryAttribute;\
FPCGMetadataAttribute<_TYPE>* SecondaryAttribute;\
inline virtual _TYPE GetPrimaryValue(const PCGMetadataAttributeKey& Key) const;\
inline virtual _TYPE GetSecondaryValue(const PCGMetadataAttributeKey& Key) const;\
virtual void Flush() override;

#define PCGEX_SAO_CLASS(_TYPE, _NAME) \
UCLASS(Blueprintable, EditInlineNew, HideDropdown) \
class PCGEXTENDEDTOOLKIT_API UPCGExBlend##_NAME##Base : public UPCGExDataBlendingOperation{\
GENERATED_BODY()\
PCGEX_SAO_BODY(_TYPE, _NAME)};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown, Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExDataBlendingOperation : public UPCGExInstruction
{
	GENERATED_BODY()

public:
	void SetAttributeName(FName InName) { AttributeName = InName; }
	FName GetAttributeName() const { return AttributeName; }

	virtual void PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData);

	virtual bool GetRequiresPreparation() const;
	virtual bool GetRequiresFinalization() const;

	virtual void PrepareOperation(const PCGMetadataEntryKey InPrimaryOutputKey) const;
	virtual void DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha = 0) const;
	virtual void FinalizeOperation(const PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const;
	virtual void ResetToDefault(const PCGMetadataEntryKey InPrimaryOutputKey) const;
	virtual void Flush();
	
protected:
	bool bInterpolationAllowed = true;
	FName AttributeName = NAME_None;
	FPCGMetadataAttributeBase* PrimaryBaseAttribute;
	FPCGMetadataAttributeBase* SecondaryBaseAttribute;
	virtual void StrongTypeAttributes();

	
	
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
class PCGEXTENDEDTOOLKIT_API UPCGExBlendBooleanBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(bool, Boolean)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger32Base : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(int32, Integer32)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendInteger64Base : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(int64, Integer64)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendFloatBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(float, Float)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendDoubleBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(double, Double)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector2Base : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FVector2D, Vector2)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVectorBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FVector, Vector)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendVector4Base : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FVector4, Vector4)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendQuaternionBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FQuat, Quaternion)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendRotatorBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FRotator, Rotator)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendTransformBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FTransform, Transform)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendStringBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FString, String)
};

UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExBlendNameBase : public UPCGExDataBlendingOperation
{
	GENERATED_BODY()
	PCGEX_SAO_BODY(FName, Name)
};

#pragma endregion

#undef PCGEX_SAO_BODY
#undef PCGEX_SAO_CLASS

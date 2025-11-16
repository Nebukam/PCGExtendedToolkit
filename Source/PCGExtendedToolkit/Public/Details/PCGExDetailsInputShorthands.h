// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExSettingsMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExDetailsInputShorthands.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType Input = EPCGExInputValueType::Constant;
};

#pragma region Name

#define PCGEX_SHORTHAND_NAME_CTR(_NAME, _TYPE) \
FPCGExInputShorthandName##_NAME() = default; \
explicit FPCGExInputShorthandName##_NAME(const FName DefaultName) { Attribute = DefaultName; } \
FPCGExInputShorthandName##_NAME(const FName DefaultName, const _TYPE DefaultValue){	Attribute = DefaultName;	Constant = DefaultValue;} \
PCGEX_SETTING_VALUE_DECL(, _TYPE)

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameBase : public FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Attribute = NAME_None;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameBoolean : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Boolean, bool)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameFloat : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Float, float)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameDouble : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Double, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameInteger32 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Integer32, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector2 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector2, FVector2D)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector4 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector4, FVector4)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameRotator : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Rotator, FRotator)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameTransform : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Transform, FTransform)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameString : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(String, FString)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameName : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Name, FName)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Constant = NAME_None;
};

#pragma endregion

#pragma region Selector

#define PCGEX_SHORTHAND_SELECTOR_CTR(_NAME, _TYPE) \
FPCGExInputShorthandSelector##_NAME() = default;\
explicit FPCGExInputShorthandSelector##_NAME(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }\
explicit FPCGExInputShorthandSelector##_NAME(const FName& DefaultSelection) { Attribute.SetAttributeName(DefaultSelection); }\
FPCGExInputShorthandSelector##_NAME(const FString& DefaultSelection, const _TYPE DefaultValue){	Attribute.Update(DefaultSelection);	Constant = DefaultValue; }\
FPCGExInputShorthandSelector##_NAME(const FName& DefaultSelection, const _TYPE DefaultValue){	Attribute.SetAttributeName(DefaultSelection);	Constant = DefaultValue; }\
PCGEX_SETTING_VALUE_DECL(, _TYPE)

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorBase : public FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorBoolean : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Boolean, bool)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorFloat : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Float, float)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorDouble : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Double, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorInteger32 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Integer32, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector2 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector2, FVector2D)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector4 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector4, FVector4)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorRotator : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Rotator, FRotator)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorTransform : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Transform, FTransform)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorString : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(String, FString)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorName : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Name, FName)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Constant = NAME_None;
};


#pragma endregion

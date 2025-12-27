// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSettingsMacros.h"
#include "Data/PCGExDataCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExInputShorthandsDetails.generated.h"

struct FPCGExContext;

namespace PCGExData
{
	class FPointIO;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType Input = EPCGExInputValueType::Constant;
};

#define PCGEX_ATTRIBUTE_TOGGLE_DECL const bool bDefaultToAttribute = false
#define PCGEX_ATTRIBUTE_TOGGLE Input = bDefaultToAttribute ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant;

#pragma region Name

#define PCGEX_SHORTHAND_NAME_CTR(_NAME, _TYPE) \
FPCGExInputShorthandName##_NAME() = default; \
explicit FPCGExInputShorthandName##_NAME(const FName DefaultName, PCGEX_ATTRIBUTE_TOGGLE_DECL) { Attribute = DefaultName; PCGEX_ATTRIBUTE_TOGGLE  } \
FPCGExInputShorthandName##_NAME(const FName DefaultName, const _TYPE DefaultValue, PCGEX_ATTRIBUTE_TOGGLE_DECL){	Attribute = DefaultName; Constant = DefaultValue; PCGEX_ATTRIBUTE_TOGGLE } \
bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet = false) const;\
bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet = false) const;\
PCGEX_SETTING_VALUE_DECL(, _TYPE)

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameBase : public FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Attribute = NAME_None;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameBoolean : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Boolean, bool)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameFloat : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Float, float)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameDouble : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Double, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameDoubleAbs : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(DoubleAbs, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameDouble01 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Double01, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameInteger32 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Integer32, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameInteger32Abs : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Integer32Abs, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0))
	int32 Constant = 0;
};


USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameInteger3201 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Integer3201, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameVector2 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector2, FVector2D)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameVector : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameDirection : public FPCGExInputShorthandNameVector
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Direction, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlip = false;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameVector4 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Vector4, FVector4)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameRotator : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Rotator, FRotator)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameTransform : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(Transform, FTransform)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameString : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_NAME_CTR(String, FString)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandNameName : public FPCGExInputShorthandNameBase
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
explicit FPCGExInputShorthandSelector##_NAME(const FString& DefaultSelection, PCGEX_ATTRIBUTE_TOGGLE_DECL) { Attribute.Update(DefaultSelection); PCGEX_ATTRIBUTE_TOGGLE } \
explicit FPCGExInputShorthandSelector##_NAME(const FName& DefaultSelection, PCGEX_ATTRIBUTE_TOGGLE_DECL) { Attribute.SetAttributeName(DefaultSelection); PCGEX_ATTRIBUTE_TOGGLE } \
FPCGExInputShorthandSelector##_NAME(const FString& DefaultSelection, const _TYPE DefaultValue, PCGEX_ATTRIBUTE_TOGGLE_DECL){ Attribute.Update(DefaultSelection);	Constant = DefaultValue; PCGEX_ATTRIBUTE_TOGGLE } \
FPCGExInputShorthandSelector##_NAME(const FName& DefaultSelection, const _TYPE DefaultValue, PCGEX_ATTRIBUTE_TOGGLE_DECL){ Attribute.Update(DefaultSelection.ToString()); Constant = DefaultValue; PCGEX_ATTRIBUTE_TOGGLE  } \
bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet = false) const;\
bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet = false) const;\
PCGEX_SETTING_VALUE_DECL(, _TYPE)

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorBase : public FPCGExInputShorthandBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorBase() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorBoolean : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Boolean, bool)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorFloat : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Float, float)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorDouble : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Double, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorDoubleAbs : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(DoubleAbs, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorDouble01 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Double01, double)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	double Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorInteger32 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Integer32, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorInteger32Abs : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Integer32Abs, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorInteger3201 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Integer3201, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	int32 Constant = 0;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorVector2 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector2, FVector2D)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorVector : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorDirection : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Direction, FVector)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlip = false;
};


USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorVector4 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Vector4, FVector4)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorRotator : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Rotator, FRotator)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorTransform : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Transform, FTransform)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorString : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(String, FString)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExInputShorthandSelectorName : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	PCGEX_SHORTHAND_SELECTOR_CTR(Name, FName)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Constant = NAME_None;
};

#pragma endregion

#undef PCGEX_ATTRIBUTE_TOGGLE
#undef PCGEX_SHORTHAND_NAME_CTR
#undef PCGEX_SHORTHAND_SELECTOR_CTR

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

	FPCGExInputShorthandNameBoolean() = default;
	explicit FPCGExInputShorthandNameBoolean(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameBoolean(const FName DefaultName, const bool DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;

	PCGEX_SETTING_VALUE_DECL(, bool)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameFloat : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameFloat() = default;
	explicit FPCGExInputShorthandNameFloat(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameFloat(const FName DefaultName, const float DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, float)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameDouble : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameDouble() = default;
	explicit FPCGExInputShorthandNameDouble(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameDouble(const FName DefaultName, const double DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, double)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameInteger32 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameInteger32() = default;
	explicit FPCGExInputShorthandNameInteger32(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameInteger32(const FName DefaultName, const int32 DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, int32)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector2 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameVector2() = default;
	explicit FPCGExInputShorthandNameVector2(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameVector2(const FName DefaultName, const FVector2D DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;

	PCGEX_SETTING_VALUE_DECL(, FVector2D)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameVector() = default;
	explicit FPCGExInputShorthandNameVector(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameVector(const FName DefaultName, const FVector& DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;

	PCGEX_SETTING_VALUE_DECL(, FVector)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameVector4 : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameVector4() = default;
	explicit FPCGExInputShorthandNameVector4(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameVector4(const FName DefaultName, const FVector4& DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);

	PCGEX_SETTING_VALUE_DECL(, FVector4)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameRotator : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameRotator() = default;
	explicit FPCGExInputShorthandNameRotator(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameRotator(const FName DefaultName, const FRotator& DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;

	PCGEX_SETTING_VALUE_DECL(, FRotator)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameTransform : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameTransform() = default;
	explicit FPCGExInputShorthandNameTransform(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameTransform(const FName DefaultName, const FTransform& DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;

	PCGEX_SETTING_VALUE_DECL(, FTransform)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameString : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameString() = default;
	explicit FPCGExInputShorthandNameString(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameString(const FName DefaultName, const FString& DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");

	PCGEX_SETTING_VALUE_DECL(, FString)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandNameName : public FPCGExInputShorthandNameBase
{
	GENERATED_BODY()

	FPCGExInputShorthandNameName() = default;
	explicit FPCGExInputShorthandNameName(const FName DefaultName) { Attribute = DefaultName; }

	FPCGExInputShorthandNameName(const FName DefaultName, const FName DefaultValue)
	{
		Attribute = DefaultName;
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Constant = NAME_None;

	PCGEX_SETTING_VALUE_DECL(, FName)
};

#pragma endregion

#pragma region Selector


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

	FPCGExInputShorthandSelectorBoolean() = default;
	explicit FPCGExInputShorthandSelectorBoolean(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorBoolean(const FString& DefaultSelection, const bool DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool Constant = false;

	PCGEX_SETTING_VALUE_DECL(, bool)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorFloat : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorFloat() = default;
	explicit FPCGExInputShorthandSelectorFloat(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorFloat(const FString& DefaultSelection, const float DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, float)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorDouble : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorDouble() = default;
	explicit FPCGExInputShorthandSelectorDouble(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorDouble(const FString& DefaultSelection, const double DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, double)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorInteger32 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorInteger32() = default;
	explicit FPCGExInputShorthandSelectorInteger32(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorInteger32(const FString& DefaultSelection, const int32 DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Constant = 0;

	PCGEX_SETTING_VALUE_DECL(, int32)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector2 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorVector2() = default;
	explicit FPCGExInputShorthandSelectorVector2(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorVector2(const FString& DefaultSelection, const FVector2D DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector2D Constant = FVector2D::ZeroVector;

	PCGEX_SETTING_VALUE_DECL(, FVector2D)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorVector() = default;
	explicit FPCGExInputShorthandSelectorVector(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorVector(const FString& DefaultSelection, const FVector& DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Constant = FVector::ZeroVector;

	PCGEX_SETTING_VALUE_DECL(, FVector)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorVector4 : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorVector4() = default;
	explicit FPCGExInputShorthandSelectorVector4(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorVector4(const FString& DefaultSelection, const FVector4& DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector4 Constant = FVector4(0, 0, 0, 1);

	PCGEX_SETTING_VALUE_DECL(, FVector4)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorRotator : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorRotator() = default;
	explicit FPCGExInputShorthandSelectorRotator(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorRotator(const FString& DefaultSelection, const FRotator& DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator Constant = FRotator::ZeroRotator;

	PCGEX_SETTING_VALUE_DECL(, FRotator)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorTransform : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorTransform() = default;
	explicit FPCGExInputShorthandSelectorTransform(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorTransform(const FString& DefaultSelection, const FTransform& DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Constant = FTransform::Identity;

	PCGEX_SETTING_VALUE_DECL(, FTransform)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorString : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorString() = default;
	explicit FPCGExInputShorthandSelectorString(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorString(const FString& DefaultSelection, const FString& DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString Constant = TEXT("");

	PCGEX_SETTING_VALUE_DECL(, FString)
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputShorthandSelectorName : public FPCGExInputShorthandSelectorBase
{
	GENERATED_BODY()

	FPCGExInputShorthandSelectorName() = default;
	explicit FPCGExInputShorthandSelectorName(const FString& DefaultSelection) { Attribute.Update(DefaultSelection); }

	FPCGExInputShorthandSelectorName(const FString& DefaultSelection, const FName DefaultValue)
	{
		Attribute.Update(DefaultSelection);
		Constant = DefaultValue;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Constant = NAME_None;

	PCGEX_SETTING_VALUE_DECL(, FName)
};


#pragma endregion 

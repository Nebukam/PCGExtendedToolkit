// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"

#include "PCGExTuple.generated.h"

class FPCGMetadataAttributeBase;

UENUM(BlueprintType)
enum class EPCGExTupleTypes : uint8
{
	Float = 0,
	Double,
	Integer32,
	Vector2,
	Vector,
	Vector4,
	Color,
	Transform,
	String,
	Boolean,
	Rotator,
	Name,
	SoftObjectPath,
	SoftClassPath
};

namespace PCGExTuple
{
	PCGEXTENDEDTOOLKIT_API
	EPCGMetadataTypes GetMetadataType(EPCGExTupleTypes Type);
}

#pragma region Singles

USTRUCT(BlueprintType)
struct FPCGExTupleValueWrap
{
	GENERATED_BODY()

	UPROPERTY()
	int32 HeaderId = 0;
	
	FPCGExTupleValueWrap() = default;
	virtual ~FPCGExTupleValueWrap() = default;
		
	virtual EPCGExTupleTypes GetValueType() const { return EPCGExTupleTypes::Float; };
	
};

USTRUCT(BlueprintType, DisplayName="Boolean")
struct FPCGExTupleValueWrapBoolean : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapBoolean() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	bool Value = false;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Boolean; };
	
};

USTRUCT(BlueprintType, DisplayName="Float")
struct FPCGExTupleValueWrapFloat : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapFloat() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Value = 0;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Float; };
	
};

USTRUCT(BlueprintType, DisplayName="Double")
struct FPCGExTupleValueWrapDouble : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapDouble() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	double Value = 0;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Double; };
	
};

USTRUCT(BlueprintType, DisplayName="Integer 32")
struct FPCGExTupleValueWrapInteger32 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapInteger32() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	int32 Value = 0;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Integer32; };
	
};

USTRUCT(BlueprintType, DisplayName="Vector2")
struct FPCGExTupleValueWrapVector2 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapVector2() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector2D Value = FVector2D::ZeroVector;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Vector2; };
	
};

USTRUCT(BlueprintType, DisplayName="Vector")
struct FPCGExTupleValueWrapVector : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapVector() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector Value = FVector::ZeroVector;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Vector; };
	
};

USTRUCT(BlueprintType, DisplayName="Vector4")
struct FPCGExTupleValueWrapVector4 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapVector4() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector4 Value = FVector4::Zero();
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Vector4; };
	
};

USTRUCT(BlueprintType, DisplayName="Color")
struct FPCGExTupleValueWrapColor : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapColor() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FLinearColor Value = FLinearColor::White;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Color; };
	
};

USTRUCT(BlueprintType, DisplayName="Transform")
struct FPCGExTupleValueWrapTransform : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapTransform() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FTransform Value = FTransform::Identity;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Transform; };
	
};

USTRUCT(BlueprintType, DisplayName="Rotator")
struct FPCGExTupleValueWrapRotator : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapRotator() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FRotator Value = FRotator::ZeroRotator;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Rotator; };
	
};

USTRUCT(BlueprintType, DisplayName="String")
struct FPCGExTupleValueWrapString : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapString() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FString Value = TEXT("");
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::String; };
	
};

USTRUCT(BlueprintType, DisplayName="Name")
struct FPCGExTupleValueWrapName : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapName() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Value = NAME_None;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::Name; };
	
};

USTRUCT(BlueprintType, DisplayName="Soft Object Path")
struct FPCGExTupleValueWrapSoftObjectPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapSoftObjectPath() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftObjectPath Value;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::SoftObjectPath; };
	
};

USTRUCT(BlueprintType, DisplayName="Soft Class Path")
struct FPCGExTupleValueWrapSoftClassPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	
	FPCGExTupleValueWrapSoftClassPath() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftClassPath Value;
	
	virtual EPCGExTupleTypes GetValueType() const override { return EPCGExTupleTypes::SoftClassPath; };
	
};
#pragma endregion

USTRUCT(BlueprintType)
struct FPCGExTupleValueHeader
{
	GENERATED_BODY()
	
	FPCGExTupleValueHeader();

	UPROPERTY()
	int32 HeaderId = 0;
	
	UPROPERTY()
	int32 Order = -1;
	
	UPROPERTY()
	EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ExcludeBaseStruct, ShowOnlyInnerProperties, FullyExpand=true))
	TInstancedStruct<FPCGExTupleValueWrap> DefaultData;

	void SanitizeEntry(TInstancedStruct<FPCGExTupleValueWrap>& InData) const;
	FPCGMetadataAttributeBase* CreateAttribute(FPCGExContext* InContext, UPCGParamData* TupleData) const;
};


USTRUCT(BlueprintType)
struct FPCGExTupleBody
{
	GENERATED_BODY()
	
	FPCGExTupleBody() = default;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, EditFixedSize, meta=(TitleProperty="{Name}", ExcludeBaseStruct, ShowOnlyInnerProperties, FullyExpand=true))
	TArray<TInstancedStruct<FPCGExTupleValueWrap>> Row;
		
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="quality-of-life/tuple"))
class UPCGExTupleSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(Tuple, "Tuple", "A Simple Tuple attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorConstant; }
	
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	
	/** Tuple composition */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{Name}", ToolTip="Tuple composition, per-row values are set in the values array."))
	TArray<FPCGExTupleValueHeader> Composition;
	
	/** Tuple composition */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ToolTip="Per-row values. Do no change the type here, it will be reset internally; instead, change it in the composition.", FullyExpand=true))
	TArray<FPCGExTupleBody> Values;
	
};

class FPCGExTupleElement final : public IPCGElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

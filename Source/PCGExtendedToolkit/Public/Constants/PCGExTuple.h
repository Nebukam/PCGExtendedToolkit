// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCompare.h"
#include "PCGExConstantEnum.h"
#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"
#include "StructUtils/InstancedStruct.h"

#include "PCGExTuple.generated.h"

class FPCGMetadataAttributeBase;

#pragma region Singles

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrap
{
	GENERATED_BODY()

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	int32 HeaderId = 0;

	UPROPERTY()
	bool bIsModel = false;

	FPCGExTupleValueWrap() = default;
	virtual ~FPCGExTupleValueWrap() = default;

	virtual FPCGMetadataAttributeBase* CreateAttribute(UPCGMetadata* Metadata, FName Name) const;

	/** Called once when row entry is initialized, because the header type has been changed */
	virtual void InitEntry(const FPCGExTupleValueWrap* InHeader);

	/** Called when the data is written to an attribute */
	virtual void WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const;

	/** Called on existing entries when a modification occurs*/
	virtual void SanitizeEntry(const FPCGExTupleValueWrap* InHeader);
};

#define PCGEX_TUPLE_WRAP_DECL(_TYPE)\
virtual FPCGMetadataAttributeBase* CreateAttribute(UPCGMetadata* Metadata, FName Name) const override;\
virtual void InitEntry(const FPCGExTupleValueWrap* InHeader);\
virtual void WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const override;

USTRUCT(BlueprintType, DisplayName="Boolean")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapBoolean : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapBoolean() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	bool Value = false;

	PCGEX_TUPLE_WRAP_DECL(Boolean)
};

USTRUCT(BlueprintType, DisplayName="Float")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapFloat : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapFloat() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Value = 0;

	PCGEX_TUPLE_WRAP_DECL(Float)
};

USTRUCT(BlueprintType, DisplayName="Double")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapDouble : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapDouble() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	double Value = 0;

	PCGEX_TUPLE_WRAP_DECL(Double)
};

USTRUCT(BlueprintType, DisplayName="Integer 32")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapInteger32 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapInteger32() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	int32 Value = 0;

	PCGEX_TUPLE_WRAP_DECL(Integer32)
};

USTRUCT(BlueprintType, DisplayName="Vector2")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapVector2 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapVector2() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector2D Value = FVector2D::ZeroVector;

	PCGEX_TUPLE_WRAP_DECL(Vector2)
};

USTRUCT(BlueprintType, DisplayName="Vector")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapVector : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapVector() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector Value = FVector::ZeroVector;

	PCGEX_TUPLE_WRAP_DECL(Vector)
};

USTRUCT(BlueprintType, DisplayName="Vector4")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapVector4 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapVector4() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector4 Value = FVector4::Zero();

	PCGEX_TUPLE_WRAP_DECL(Vector4)
};

USTRUCT(BlueprintType, DisplayName="Color")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapColor : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapColor() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FLinearColor Value = FLinearColor::White;

	PCGEX_TUPLE_WRAP_DECL(Color)
};

USTRUCT(BlueprintType, DisplayName="Transform")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapTransform : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapTransform() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FTransform Value = FTransform::Identity;

	PCGEX_TUPLE_WRAP_DECL(Transform)
};

USTRUCT(BlueprintType, DisplayName="Rotator")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapRotator : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapRotator() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FRotator Value = FRotator::ZeroRotator;

	PCGEX_TUPLE_WRAP_DECL(Rotator)
};

USTRUCT(BlueprintType, DisplayName="String")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapString : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapString() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FString Value = TEXT("");

	PCGEX_TUPLE_WRAP_DECL(String)
};

USTRUCT(BlueprintType, DisplayName="Name")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapName : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapName() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Value = NAME_None;

	PCGEX_TUPLE_WRAP_DECL(Name)
};

USTRUCT(BlueprintType, DisplayName="Soft Object Path")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapSoftObjectPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapSoftObjectPath() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftObjectPath Value;

	PCGEX_TUPLE_WRAP_DECL(SoftObjectPath)
};

USTRUCT(BlueprintType, DisplayName="Soft Class Path")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapSoftClassPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapSoftClassPath() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftClassPath Value;

	PCGEX_TUPLE_WRAP_DECL(SoftClassPath)
};

USTRUCT(BlueprintType, DisplayName="Enum Selector")
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueWrapEnumSelector : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	FPCGExTupleValueWrapEnumSelector() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FEnumSelector Enum;

	PCGEX_TUPLE_WRAP_DECL(EnumSelector)

	virtual void SanitizeEntry(const FPCGExTupleValueWrap* InHeader) override;
};

#pragma endregion

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleValueHeader
{
	GENERATED_BODY()

	FPCGExTupleValueHeader();

	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	int32 HeaderId = 0;

	UPROPERTY()
	int32 Order = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ExcludeBaseStruct, ShowOnlyInnerProperties, FullyExpand=true))
	TInstancedStruct<FPCGExTupleValueWrap> DefaultData;

	void SanitizeEntry(TInstancedStruct<FPCGExTupleValueWrap>& InData) const;
	FPCGMetadataAttributeBase* CreateAttribute(FPCGExContext* InContext, UPCGParamData* TupleData) const;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTupleBody
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
	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(false)
};

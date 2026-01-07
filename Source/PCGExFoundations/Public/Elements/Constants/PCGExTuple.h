// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExConstantEnum.h"
#include "PCGSettings.h"
#include "StructUtils/InstancedStruct.h"

#include "PCGExTuple.generated.h"

class FPCGMetadataAttributeBase;

#pragma region Singles

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrap
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

#define PCGEX_TUPLEVALUEWRAP_CTR(_TYPE)\
virtual FPCGMetadataAttributeBase* CreateAttribute(UPCGMetadata* Metadata, FName Name) const override;\
virtual void InitEntry(const FPCGExTupleValueWrap* InHeader);\
virtual void WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const override;\
FPCGExTupleValueWrap##_TYPE() = default;

USTRUCT(BlueprintType, DisplayName="Boolean")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapBoolean : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	bool Value = false;

	PCGEX_TUPLEVALUEWRAP_CTR(Boolean)
};

USTRUCT(BlueprintType, DisplayName="Float")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapFloat : public FPCGExTupleValueWrap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Value = 0;

	PCGEX_TUPLEVALUEWRAP_CTR(Float)
};

USTRUCT(BlueprintType, DisplayName="Double")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapDouble : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Double)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	double Value = 0;
};

USTRUCT(BlueprintType, DisplayName="Integer 32")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapInteger32 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Integer32)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	int32 Value = 0;
};

USTRUCT(BlueprintType, DisplayName="Vector2")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapVector2 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Vector2)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector2D Value = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType, DisplayName="Vector")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapVector : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Vector)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector Value = FVector::ZeroVector;
};

USTRUCT(BlueprintType, DisplayName="Vector4")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapVector4 : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Vector4)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FVector4 Value = FVector4::Zero();
};

USTRUCT(BlueprintType, DisplayName="Color")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapColor : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Color)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType, DisplayName="Transform")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapTransform : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Transform)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FTransform Value = FTransform::Identity;
};

USTRUCT(BlueprintType, DisplayName="Rotator")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapRotator : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Rotator)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FRotator Value = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType, DisplayName="String")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapString : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(String)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FString Value = TEXT("");
};

USTRUCT(BlueprintType, DisplayName="Name")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapName : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(Name)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FName Value = NAME_None;
};

USTRUCT(BlueprintType, DisplayName="Soft Object Path")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapSoftObjectPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(SoftObjectPath)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftObjectPath Value;
};

USTRUCT(BlueprintType, DisplayName="Soft Class Path")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapSoftClassPath : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(SoftClassPath)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FSoftClassPath Value;
};

USTRUCT(BlueprintType, DisplayName="Enum Selector")
struct PCGEXFOUNDATIONS_API FPCGExTupleValueWrapEnumSelector : public FPCGExTupleValueWrap
{
	GENERATED_BODY()
	PCGEX_TUPLEVALUEWRAP_CTR(EnumSelector)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FEnumSelector Enum;

	virtual void SanitizeEntry(const FPCGExTupleValueWrap* InHeader) override;
};

#pragma endregion

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExTupleValueHeader
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
struct PCGEXFOUNDATIONS_API FPCGExTupleBody
{
	GENERATED_BODY()

	FPCGExTupleBody() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, EditFixedSize, meta=(TitleProperty="{Name}", ExcludeBaseStruct, ShowOnlyInnerProperties, FullyExpand=true, ForceInlineRow))
	TArray<TInstancedStruct<FPCGExTupleValueWrap>> Row;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="quality-of-life/tuple"))
class UPCGExTupleSettings : public UPCGExSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Tuple, "Tuple", "A Simple Tuple attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Constant); }

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

	/** Tuple values */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ToolTip="Per-row values. Do no change the type here, it will be reset internally; instead, change it in the composition.", FullyExpand=true))
	TArray<FPCGExTupleBody> Values;

	/** A list of tags separated by a comma, for easy overrides. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedTags;
};

class FPCGExTupleElement final : public IPCGExElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

#undef PCGEX_TUPLEVALUEWRAP_CTR

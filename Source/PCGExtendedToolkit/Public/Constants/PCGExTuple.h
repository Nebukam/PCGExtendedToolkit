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
	
USTRUCT(BlueprintType)
struct FPCGExTupleValue
{
	GENERATED_BODY()
	
	FPCGExTupleValue() = default;

	UPROPERTY()
	bool bIsHeader = false;

	UPROPERTY()
	int32 EDITOR_GUID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="!bIsHeader", EditConditionHides, HideEditConditionToggle))
	bool bUseDefaultValue = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="bIsHeader", EditConditionHides, HideEditConditionToggle))
	FName Name = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="bIsHeader", EditConditionHides, HideEditConditionToggle))
	EPCGExTupleTypes Type = EPCGExTupleTypes::Float;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Float && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	float FloatValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Double && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	double DoubleValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Integer32 && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	int32 Integer32Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector2 && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FVector2D Vector2Value = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FVector VectorValue = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector4 && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FVector4 Vector4Value = FVector4::Zero();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Color && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FLinearColor ColorValue = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Transform && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FTransform TransformValue = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::String && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FString StringValue = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Boolean && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	bool BooleanValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Rotator && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FRotator RotatorValue = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Name && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FName NameValue = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::SoftObjectPath && !bUseDefaultValue", EditConditionHides))
	FSoftObjectPath SoftObjectPathValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::SoftClassPath && !bUseDefaultValue", EditConditionHides, HideEditConditionToggle))
	FSoftClassPath SoftClassPathValue;
		
};

USTRUCT(BlueprintType)
struct FPCGExTupleValueHeader : public  FPCGExTupleValue
{
	GENERATED_BODY()
	
	FPCGExTupleValueHeader();
	
	UPROPERTY()
	int32 Order = -1;
	
	UPROPERTY()
	EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

	FPCGMetadataAttributeBase* CreateAttribute(FPCGExContext* InContext, UPCGParamData* TupleData) const;
};


USTRUCT(BlueprintType)
struct FPCGExTupleBody
{
	GENERATED_BODY()
	
	FPCGExTupleBody() = default;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(TitleProperty="{Name}"), EditFixedSize)
	TArray<FPCGExTupleValue> Values;
		
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{Name}", ToolTip="A Header"))
	TArray<FPCGExTupleValueHeader> Composition;
	
	/** Tuple composition */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ToolTip="A Row"))
	TArray<FPCGExTupleBody> Values;
	
};

class FPCGExTupleElement final : public IPCGElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

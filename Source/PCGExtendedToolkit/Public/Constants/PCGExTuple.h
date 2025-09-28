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
	EPCGExTupleTypes Type = EPCGExTupleTypes::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Float", EditConditionHides))
	float FloatValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Double", EditConditionHides))
	double DoubleValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Integer32", EditConditionHides))
	int32 Integer32Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector2", EditConditionHides))
	FVector2D Vector2Value = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector", EditConditionHides))
	FVector VectorValue = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Vector4", EditConditionHides))
	FVector4 Vector4Value = FVector4::Zero();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Color", EditConditionHides))
	FLinearColor ColorValue = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Transform", EditConditionHides))
	FTransform TransformValue = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::String", EditConditionHides))
	FString StringValue = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Boolean", EditConditionHides))
	bool BooleanValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Rotator", EditConditionHides))
	FRotator RotatorValue = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::Name", EditConditionHides))
	FName NameValue = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::SoftObjectPath", EditConditionHides))
	FSoftObjectPath SoftObjectPathValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(EditCondition="Type==EPCGExTupleTypes::SoftClassPath", EditConditionHides))
	FSoftClassPath SoftClassPathValue;
		
};

USTRUCT(BlueprintType)
struct FPCGExTupleValues
{
	GENERATED_BODY()
	
	FPCGExTupleValues() = default;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExTupleTypes Type = EPCGExTupleTypes::Float;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(TitleProperty="Value"))
	TArray<FPCGExTupleValue> Values;

	FPCGMetadataAttributeBase* Write(FName Name, UPCGParamData* ParamData) const;
	void WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key, int32 Index) const;
		
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="TBD"))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TMap<FName, FPCGExTupleValues> Composition;
	
#if WITH_EDITOR
	/** Set the same number of entries to all value arrays */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Equalize", ShortToolTip="Set the same number of entries to all value arrays", DisplayOrder=2))
	void EDITOR_Equalize();
	
	/** Add an entry to all keys. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="+1", ShortToolTip="Add an entry to all keys.", DisplayOrder=2))
	void EDITOR_AddOneToAll();
#endif
};

class FPCGExTupleElement final : public IPCGElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "PCGExAttributeBlendFactoryProvider.generated.h"

UENUM()
enum class EPCGExOperandAuthority : uint8
{
	A = 0 UMETA(DisplayName = "Operand A", ToolTip="Type of operand A will drive the output type, thus converting operand B to the same type for the operation."),
	B = 1 UMETA(DisplayName = "Operand B", ToolTip="Type of operand B will drive the output type, thus converting operand A to the same type for the operation."),
	Custom = 2 UMETA(DisplayName = "Custom", ToolTip="Select a specific type to output the result to."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeBlendConfig
{
	GENERATED_BODY()

	FPCGExAttributeBlendConfig()
	{
		LocalWeightCurve.EditorCurveData.AddKey(0, 0);
		LocalWeightCurve.EditorCurveData.AddKey(1, 1);
		OutputTo.Update("Result");
	}

	~FPCGExAttributeBlendConfig() = default;

	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	bool bRequiresWeight = true;

	/** Blendmode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDataBlendingType BlendMode = EPCGExDataBlendingType::Lerp;

	/** Operand A. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Operand B. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandB;

	/** Output to (AB blend). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OutputTo;

	/** Which type should be used for the output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandAuthority OutputType = EPCGExOperandAuthority::A;

	/** Which type should be used for the output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Type",EditCondition="OutputType==EPCGExOperandAuthority::Custom", EditConditionHides))
	EPCGMetadataTypes CustomType = EPCGMetadataTypes::Double;
	
	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight (Attr)", EditCondition="bRequiresWeight && WeightInput!=EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Constant weight value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="bRequiresWeight && WeightInput==EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	double Weight = 0.5;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_NotOverridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable, DisplayName="Weight Curve", EditCondition = "bRequiresWeight && bUseLocalCurve", EditConditionHides, HideEditConditionToggle))
	FRuntimeFloatCurve LocalWeightCurve;

	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight Curve", EditCondition="bRequiresWeight && !bUseLocalCurve", EditConditionHides, HideEditConditionToggle))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	const FRichCurve* ScoreCurveObj = nullptr;

	void Init();
};

/**
 * 
 */
UCLASS(DisplayName = "Blending")
class PCGEXTENDEDTOOLKIT_API UPCGExAttributeBlendOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExAttributeBlendConfig Config;

	virtual bool PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade);
	virtual void BlendScope(const PCGExMT::FScope& InScope);
	virtual void Cleanup() override;

protected:
	TSharedPtr<PCGExDataBlending::FDataBlendingProcessorBase> BlendingProcessor;
	TSharedPtr<PCGExData::FBufferBase> Buffer_A;
	TSharedPtr<PCGExData::FBufferBase> Buffer_B;
	TSharedPtr<PCGExData::FBufferBase> Buffer_Target;
};

UCLASS(Hidden, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class PCGEXTENDEDTOOLKIT_API UPCGExAttributeBlendFactory : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	FPCGExAttributeBlendConfig Config;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Blending; }
	virtual UPCGExAttributeBlendOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class PCGEXTENDEDTOOLKIT_API UPCGExAttributeBlendFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Blending, "Blending", "Creates a single AttributeBlend computational node, to be used with Blendings pins.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExDataBlending::OutputBlendingLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Config. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeBlendConfig Config;

protected:
	virtual bool IsCacheable() const override { return true; }
};

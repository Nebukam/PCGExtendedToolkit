// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBlendingCommon.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "Factories/PCGExFactoryProvider.h"
#include "Factories/PCGExOperation.h"
#include "PCGExProxyDataBlending.h"
#include "Details/PCGExSettingsMacros.h"
#include "Factories/PCGExFactoryData.h"
#include "Utils/PCGExCurveLookup.h"

#include "PCGExBlendOpFactory.generated.h"

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

namespace PCGExBlending
{
	class FProxyDataBlender;

	const FName PreviousAttributeName = TEXT("#Previous");
	const FName PreviousNameAttributeName = TEXT("#PreviousName");
}

UENUM()
enum class EPCGExOperandSource : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Use a constant, user-defined value.", ActionIcon="Constant"),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Read the value from the input data.", ActionIcon="Attribute"),
};

UENUM()
enum class EPCGExOperandAuthority : uint8
{
	A      = 0 UMETA(DisplayName = "Operand A", ToolTip="Type of operand A will drive the output type, thus converting operand B to the same type for the operation."),
	B      = 1 UMETA(DisplayName = "Operand B", ToolTip="Type of operand B will drive the output type, thus converting operand A to the same type for the operation."),
	Custom = 2 UMETA(DisplayName = "Custom", ToolTip="Select a specific type to output the result to."),
	Auto   = 3 UMETA(DisplayName = "Auto", ToolTip="Takes an informed guess based on settings & existing data. Usually works well, but not fool-proof."),
};

UENUM()
enum class EPCGExBlendOpOutputMode : uint8
{
	SameAsA   = 0 UMETA(DisplayName = "Same as A", ToolTip="Will write the output value to Operand A' selector."),
	SameAsB   = 1 UMETA(DisplayName = "Same as B", ToolTip="Will write the output value to Operand B' selector. (Will default to A if B is not set)"),
	New       = 2 UMETA(DisplayName = "New", ToolTip="Will write the output value to new selector."),
	Transient = 3 UMETA(DisplayName = "New (Transient)", ToolTip="Will write the output value to a new selector that will only exist for the duration of the blend, but can be referenced by other blend ops."),
};

USTRUCT(BlueprintType)
struct PCGEXBLENDING_API FPCGExAttributeBlendWeight
{
	GENERATED_BODY()

	FPCGExAttributeBlendWeight()
	{
		LocalWeightCurve.EditorCurveData.AddKey(0, 0);
		LocalWeightCurve.EditorCurveData.AddKey(1, 1);
	}

	~FPCGExAttributeBlendWeight() = default;

	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight (Attr)", EditCondition="WeightInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Constant weight value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Weight = 0.5;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Weight Curve", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightCurve;

	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight Curve", EditCondition="!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGExCurves::WeightDistributionLinear);

	PCGExFloatLUT ScoreLUT = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	FPCGExCurveLookupDetails WeightCurveLookup;

	void Init();

	PCGEX_SETTING_VALUE_DECL(Weight, double)
};

USTRUCT(BlueprintType)
struct PCGEXBLENDING_API FPCGExAttributeBlendConfig
{
	GENERATED_BODY()

	FPCGExAttributeBlendConfig()
	{
		OperandA.Update("@Last");
		OperandB.Update("@Last");
		OutputTo.Update("Result");
	}

	~FPCGExAttributeBlendConfig() = default;

	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	bool bRequiresWeight = false;

	/** BlendMode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExABBlendingType BlendMode = EPCGExABBlendingType::Average;

	/** Operand A Source. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExOperandSource OperandASource = EPCGExOperandSource::Attribute;

	/** Operand A. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bUseOperandB = false;

	/** Operand B. If not enabled, will re-use Operand A' input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseOperandB"))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B Source. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Inline", EditCondition="bUseOperandB", HideEditConditionToggle))
	EPCGExOperandSource OperandBSource = EPCGExOperandSource::Attribute;

	/** Choose where to output the result of the A/B blend */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBlendOpOutputMode OutputMode = EPCGExBlendOpOutputMode::SameAsA;

	/** Output to (AB blend). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Output To", EditCondition="OutputMode == EPCGExBlendOpOutputMode::New || OutputMode == EPCGExBlendOpOutputMode::Transient", EditConditionHides))
	FPCGAttributePropertyInputSelector OutputTo;

	/** If enabled, when a node uses multiple sources for blending, the value will be reset to 0 for some specific BlendModes so as to not account for inherited values. Default is true, as it is usually the most desirable behavior. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bResetValueBeforeMultiSourceBlend = true;

	/** Which type should be used for the output value. Only used if the output is not a point property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable), AdvancedDisplay)
	EPCGExOperandAuthority OutputType = EPCGExOperandAuthority::Auto;

	/** Which type should be used for the output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Type", EditCondition="OutputType == EPCGExOperandAuthority::Custom", EditConditionHides), AdvancedDisplay)
	EPCGMetadataTypes CustomType = EPCGMetadataTypes::Double;

	/** Weight settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	FPCGExAttributeBlendWeight Weighting;

	void Init();
};

/**
 * 
 */
class PCGEXBLENDING_API FPCGExBlendOperation : public FPCGExOperation
{
public:
	FPCGExAttributeBlendConfig Config;

	TSharedPtr<PCGExData::FFacade> WeightFacade;

	TSharedPtr<PCGExData::FFacade> Source_A_Facade;
	PCGExData::EIOSide SideA = PCGExData::EIOSide::In;

	TSharedPtr<PCGExData::FFacade> Source_B_Facade;
	PCGExData::EIOSide SideB = PCGExData::EIOSide::In;

	TSharedPtr<PCGExData::FFacade> TargetFacade;

	TSharedPtr<PCGExData::FFacade> ConstantA;
	TSharedPtr<PCGExData::FFacade> ConstantB;

	bool bUsedForMultiBlendOnly = false;

	int32 OpIdx = -1;
	TSharedPtr<TArray<TSharedPtr<FPCGExBlendOperation>>> SiblingOperations;

	virtual bool PrepareForData(FPCGExContext* InContext);

	virtual void BlendAutoWeight(const int32 SourceIndex, const int32 TargetIndex);
	virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight);
	virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double InWeight);

	virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex);
	virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight, PCGEx::FOpStats& Tracker);
	virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker);

	virtual void CompleteWork(TSet<TSharedPtr<PCGExData::IBuffer>>& OutDisabledBuffers);

protected:
	bool CopyAndFixSiblingSelector(FPCGExContext* InContext, FPCGAttributePropertyInputSelector& Selector) const;

	TSharedPtr<PCGExDetails::TSettingValue<double>> Weight;
	TSharedPtr<PCGExBlending::FProxyDataBlender> Blender;
};

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Blend Op"))
struct FPCGExDataTypeInfoBlendOp : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXBLENDING_API)
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class PCGEXBLENDING_API UPCGExBlendOpFactory : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoBlendOp)

	FPCGExAttributeBlendConfig Config;
	TSharedPtr<PCGExData::FFacade> ConstantA;
	TSharedPtr<PCGExData::FFacade> ConstantB;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Blending; }
	virtual TSharedPtr<FPCGExBlendOperation> CreateOperation(FPCGExContext* InContext) const;

	virtual bool WantsPreparation(FPCGExContext* InContext) override;

	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override;

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;

	virtual void RegisterBuffersDependenciesForSourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;
	virtual void RegisterBuffersDependenciesForSourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;
};

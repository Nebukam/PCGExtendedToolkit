// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExProxyDataBlending.h"

#include "PCGExAttributeBlendFactoryProvider.generated.h"

namespace PCGExDataBlending
{
	class FProxyDataBlenderBase;
}

UENUM()
enum class EPCGExOperandAuthority : uint8
{
	A      = 0 UMETA(DisplayName = "Operand A", ToolTip="Type of operand A will drive the output type, thus converting operand B to the same type for the operation."),
	B      = 1 UMETA(DisplayName = "Operand B", ToolTip="Type of operand B will drive the output type, thus converting operand A to the same type for the operation."),
	Custom = 2 UMETA(DisplayName = "Custom", ToolTip="Select a specific type to output the result to."),
	Auto   = 3 UMETA(DisplayName = "Auto", ToolTip="Automatically detect the output type based on context, and fallbacks to A."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeBlendWeight
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight (Attr)", EditCondition="WeightInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Constant weight value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput==EPCGExInputValueType::Constant", EditConditionHides))
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
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	const FRichCurve* ScoreCurveObj = nullptr;

	void Init();
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeBlendConfig
{
	GENERATED_BODY()

	FPCGExAttributeBlendConfig()
	{
		OperandA.Update(PCGEx::PreviousAttributeName.ToString());
		OperandB.Update("@Last");
		OutputTo.Update("Result");
	}

	~FPCGExAttributeBlendConfig() = default;

	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	bool bRequiresWeight = true;

	/** Blendmode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExABBlendingType BlendMode = EPCGExABBlendingType::Lerp;

	/** Operand A. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Operand B. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandB;

	/** Weight settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	FPCGExAttributeBlendWeight Weighting;

	/** Output to (AB blend). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OutputTo;

	/** Which type should be used for the output value. Only used if the output is not a point property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandAuthority OutputType = EPCGExOperandAuthority::Auto;

	/** Which type should be used for the output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Type", EditCondition="OutputType==EPCGExOperandAuthority::Custom", EditConditionHides))
	EPCGMetadataTypes CustomType = EPCGMetadataTypes::Double;

	/** If enabled, new attributes will only be created for the duration of the blend, and properties will be restored to their original values once the blend is complete. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bTransactional = false;

	void Init();
};

/**
 * 
 */
UCLASS(DisplayName = "Blend Op")
class PCGEXTENDEDTOOLKIT_API UPCGExAttributeBlendOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExAttributeBlendConfig Config;

	int32 OpIdx = -1;
	TSharedPtr<TArray<UPCGExAttributeBlendOperation*>> SiblingOperations;


	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);

	virtual void Blend(const int32 Index, FPCGPoint& Point)
	{
		Blender->Blend(Index, Point, Config.Weighting.ScoreCurveObj->Eval(Weight ? Weight->Read(Index) : Config.Weighting.Weight));
	}

	virtual void CompleteWork(); 
	
	virtual void Cleanup() override;

protected:
	bool CopyAndFixSiblingSelector(FPCGExContext* InContext, FPCGAttributePropertyInputSelector& Selector) const;

	TSharedPtr<PCGExData::TBuffer<double>> Weight;
	TSharedPtr<PCGExDataBlending::FProxyDataBlenderBase> Blender;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class PCGEXTENDEDTOOLKIT_API UPCGExAttributeBlendFactory : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	FPCGExAttributeBlendConfig Config;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Blending; }
	virtual UPCGExAttributeBlendOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
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
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		BlendOp, "BlendOp", "Creates a single Blend Operation node, to be used with the Attribute Blender.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
	//PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, true)

#if PCGEX_ENGINE_VERSION > 503
	virtual bool CanUserEditTitle() const override { return false; }
#endif
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;

#endif
	//~End UPCGSettings

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	virtual FName GetMainOutputPin() const override { return PCGExDataBlending::OutputBlendingLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority;

	/** Config. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeBlendConfig Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};

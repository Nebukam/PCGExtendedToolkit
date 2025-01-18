// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "PCGExAttributeBlendFactoryProvider.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeBlendConfig
{
	GENERATED_BODY()

	FPCGExAttributeBlendConfig()
	{
		LocalWeightCurve.EditorCurveData.AddKey(0, 0);
		LocalWeightCurve.EditorCurveData.AddKey(1, 1);
	}

	~FPCGExAttributeBlendConfig()
	{
	}

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

	/** Output to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OutputTo = FName("Result");

	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Constant weight value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight", EditCondition="bRequiresWeight && WeightInput==EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	double Weight = 0.5;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight", EditCondition="bRequiresWeight && WeightInput!=EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bRequiresWeight", EditConditionHides, HideEditConditionToggle))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Weight Curve", EditCondition = "bRequiresWeight && bUseLocalCurve", EditConditionHides, HideEditConditionToggle))
	FRuntimeFloatCurve LocalWeightCurve;

	/** Curve the weight value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight Curve", EditCondition="bRequiresWeight && !bUseLocalCurve", EditConditionHides, HideEditConditionToggle))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	const FRichCurve* ScoreCurveObj = nullptr;

	void Init();
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Blending")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeBlendOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExAttributeBlendConfig Config;

	virtual void PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade);

	virtual void Cleanup() override
	{
		Super::Cleanup();
	}
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeBlendFactory : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	FPCGExAttributeBlendConfig Config;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Blending; }
	virtual UPCGExAttributeBlendOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeBlendFactoryProviderSettings : public UPCGExFactoryProviderSettings
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

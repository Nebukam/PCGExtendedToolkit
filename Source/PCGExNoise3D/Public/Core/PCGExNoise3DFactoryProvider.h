// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExNoise3DCommon.h"
#include "Factories/PCGExFactoryProvider.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "Factories/PCGExFactoryData.h"
#include "Utils/PCGExCurveLookup.h"

#include "PCGExNoise3DFactoryProvider.generated.h"

#define PCGEX_NOISE3D_FACTORY_BOILERPLATE \
virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;

#define PCGEX_NOISE3D_FACTORY_BOILERPLATE_IMPL(_TYPE, _REGISTER_ASSET_BODY)\
void UPCGExNoise3DFactory##_TYPE::RegisterAssetDependencies(FPCGExContext* InContext) const{\
	Super::RegisterAssetDependencies(InContext); }

#define PCGEX_FORWARD_NOISE3D_FACTORY \
	NewFactory->Priority = Priority; \
	NewFactory->Config = Config; \
	NewFactory->Config.Init(); \
	NewFactory->ConfigBase = NewFactory->Config;

#define PCGEX_FORWARD_NOISE3D_CONFIG \
	NewOperation->BlendMode = Config.BlendMode; \
	NewOperation->WeightFactor = Config.WeightFactor; \
	NewOperation->RemapLUT = Config.RemapLUT; \
	NewOperation->bApplyTransform = Config.bApplyTransform; \
	NewOperation->Transform = Config.Transform;

class FPCGExNoise3DOperation;

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Noise3D"))
struct FPCGExDataTypeInfoNoise3D : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXNOISE3D_API)
};

USTRUCT(BlueprintType)
struct PCGEXNOISE3D_API FPCGExNoise3DConfigBase
{
	GENERATED_BODY()

	FPCGExNoise3DConfigBase()
	{
		LocalRemapCurve.EditorCurveData.AddKey(0, 0);
		LocalRemapCurve.EditorCurveData.AddKey(1, 1);
	}

	~FPCGExNoise3DConfigBase()
	{
	}

	/** The weight factor for this Noise3D (used when combining multiple noise sources). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	double WeightFactor = 1;

	/** Blend mode when stacked against other noises */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	EPCGExNoiseBlendMode BlendMode = EPCGExNoiseBlendMode::Blend;

	/** Invert the noise output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	bool bInvert = false;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalCurve = false;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Remap Curve", EditCondition = "bUseLocalCurve", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	FRuntimeFloatCurve LocalRemapCurve;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Remap Curve", EditCondition="!bUseLocalCurve", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGExCurves::WeightDistributionLinear);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayPriority=-1))
	FPCGExCurveLookupDetails RemapCurveLookup;

	PCGExFloatLUT RemapLUT = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bApplyTransform = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bApplyTransform"))
	FTransform Transform = FTransform::Identity;

	void Init();
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXNOISE3D_API UPCGExNoise3DFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoNoise3D)

	FPCGExNoise3DConfigBase ConfigBase;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Noise3D; }

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual TSharedPtr<FPCGExNoise3DOperation> CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Noise")
class PCGEXNOISE3D_API UPCGExNoise3DFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoNoise3D)

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AbstractNoise3D, "Noise3D Definition", "Creates a single Noise3D computational node, to be used with nodes that support it.")
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Noise3D); }
#endif

	virtual FName GetMainOutputPin() const override { return PCGExNoise3D::Labels::OutputNoise3DLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Noise priority, matters for blending and weighting.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1), AdvancedDisplay)
	int32 Priority = 0;
};

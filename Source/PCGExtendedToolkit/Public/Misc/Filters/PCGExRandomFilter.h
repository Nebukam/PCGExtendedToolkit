// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "PCGExRandom.h"


#include "PCGExRandomFilter.generated.h"

USTRUCT(BlueprintType)
struct FPCGExRandomFilterConfig
{
	GENERATED_BODY()

	FPCGExRandomFilterConfig()
	{
		LocalWeightCurve.EditorCurveData.AddKey(0, 0);
		LocalWeightCurve.EditorCurveData.AddKey(1, 1);
	}

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 RandomSeed = 42;

	/** Type of Threshold value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Pass threshold -- Value is expected to fit within a 0-1 range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold (Attr)", EditCondition="ThresholdInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** Whether to normalize the threshold internally or not. Enable this if your per-point threshold does not fit within a 0-1 range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Remap to 0..1", EditCondition="ThresholdInput!=EPCGExInputValueType::Constant", EditConditionHides))
	bool bRemapThresholdInternally = false;

	/** Pass threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", EditCondition="ThresholdInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1))
	double Threshold = 0.5;

	PCGEX_SETTING_VALUE_GET(Threshold, double, ThresholdInput, ThresholdAttribute, Threshold)

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bPerPointWeight = false;

	/** Per-point weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPerPointWeight"))
	FPCGAttributePropertyInputSelector Weight;

	/** Whether to normalize the weights internally or not. Enable this if your per-point weight does not fit within a 0-1 range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Remap to 0..1", EditCondition="bPerPointWeight", HideEditConditionToggle, EditConditionHides))
	bool bRemapWeightInternally = false;

	PCGEX_SETTING_VALUE_GET(Weight, double, bPerPointWeight ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, Weight, 1)

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Weight Curve", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightCurve;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight Curve", EditCondition="!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertResult = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExRandomFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExRandomFilterConfig Config;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual bool SupportsCollectionEvaluation() const override;
	virtual bool SupportsPointEvaluation() const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExPointFilter
{
	class FRandomFilter final : public FSimpleFilter
	{
	public:
		explicit FRandomFilter(const TObjectPtr<const UPCGExRandomFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition), RandomSeed(InDefinition->Config.RandomSeed)
		{
		}

		const TObjectPtr<const UPCGExRandomFilterFactory> TypedFilterFactory;

		int32 RandomSeed;

		TSharedPtr<PCGExDetails::TSettingValue<double>> WeightBuffer;
		TSharedPtr<PCGExDetails::TSettingValue<double>> ThresholdBuffer;

		double WeightOffset = 0;
		double WeightRange = 1;

		double Threshold = 0.5;

		double ThresholdOffset = 0;
		double ThresholdRange = 1;

		const FRichCurve* WeightCurve = nullptr;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;
		virtual bool TestRoamingPoint(const FPCGPoint& Point) const override;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;


		virtual ~FRandomFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExRandomFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(RandomCompareFilterFactory, "Filter : Random", "Filter using a random value.", PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRandomFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};

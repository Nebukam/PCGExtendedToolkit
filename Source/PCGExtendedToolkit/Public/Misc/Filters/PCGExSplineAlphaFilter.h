// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExDetailsData.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSplineInclusionFilter.h"
#include "Data/PCGSplineData.h"


#include "Sampling/PCGExSampleNearestSpline.h"


#include "PCGExSplineAlphaFilter.generated.h"

UENUM()
enum class EPCGExSplineTimeConsolidation : uint8
{
	Min     = 0 UMETA(DisplayName = "Min", Tooltip="..."),
	Max     = 1 UMETA(DisplayName = "Max", Tooltip="..."),
	Average = 2 UMETA(DisplayName = "Average", Tooltip="...")
};

USTRUCT(BlueprintType)
struct FPCGExSplineAlphaFilterConfig
{
	GENERATED_BODY()

	FPCGExSplineAlphaFilterConfig()
	{
	}

	/** Sample inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	/** If a point is both inside and outside a spline (if there are multiple ones), decide what value to favor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSplineFilterPick Pick = EPCGExSplineFilterPick::Closest;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Pick != EPCGExSplineFilterPick::Closest", EditConditionHides))
	EPCGExSplineTimeConsolidation TimeConsolidation = EPCGExSplineTimeConsolidation::Min;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B (Attr)", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst == EPCGExInputValueType::Constant", EditConditionHides))
	double OperandBConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	PCGEX_SETTING_VALUE_GET(OperandB, double, CompareAgainst, OperandB, OperandBConstant)
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExSplineAlphaFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExSplineAlphaFilterConfig Config;

	TSharedPtr<TArray<const FPCGSplineStruct*>> Splines;
	TSharedPtr<TArray<double>> SegmentsNum;

	virtual bool SupportsCollectionEvaluation() const override { return false; }
	virtual bool SupportsProxyEvaluation() const override;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual bool WantsPreparation(FPCGExContext* InContext) override;
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointFilter
{
	class FSplineAlphaFilter final : public ISimpleFilter
	{
	public:
		explicit FSplineAlphaFilter(const TObjectPtr<const UPCGExSplineAlphaFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = TypedFilterFactory->Splines;
			SegmentsNum = TypedFilterFactory->SegmentsNum;
		}

		const TObjectPtr<const UPCGExSplineAlphaFilterFactory> TypedFilterFactory;

		TSharedPtr<TArray<const FPCGSplineStruct*>> Splines;
		TSharedPtr<TArray<double>> SegmentsNum;

		TSharedPtr<PCGExDetails::TSettingValue<double>> OperandB;

		TConstPCGValueRange<FTransform> InTransforms;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FSplineAlphaFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/spatial/spline-alpha"))
class UPCGExSplineAlphaFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		SplineAlphaFilterFactory, "Filter : Spline Alpha", "Creates a filter definition that checks points position against a spline' closest alpha.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSplineAlphaFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

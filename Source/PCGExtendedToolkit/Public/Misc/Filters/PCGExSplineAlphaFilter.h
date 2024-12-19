// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplineAlphaFilterConfig
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	double OperandBConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineAlphaFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSplineAlphaFilterConfig Config;
	TArray<const FPCGSplineStruct*> Splines;
	TArray<double> SegmentsNum;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;

	virtual void RegisterConsumableAttributes(FPCGExContext* InContext) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TSplineAlphaFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TSplineAlphaFilter(const TObjectPtr<const UPCGExSplineAlphaFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = TypedFilterFactory->Splines;
			SegmentsNum = TypedFilterFactory->SegmentsNum;
		}

		const TObjectPtr<const UPCGExSplineAlphaFilterFactory> TypedFilterFactory;

		TArray<const FPCGSplineStruct*> Splines;
		TArray<double> SegmentsNum;

		TSharedPtr<PCGExData::TBuffer<double>> OperandB;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TSplineAlphaFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineAlphaFilterProviderSettings : public UPCGExFilterProviderSettings
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

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

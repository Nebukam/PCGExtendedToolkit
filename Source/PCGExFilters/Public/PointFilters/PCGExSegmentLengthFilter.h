// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGExCompare.h"
#include "Core/PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"
#include "Math/PCGExMath.h"

#include "PCGExSegmentLengthFilter.generated.h"

USTRUCT(BlueprintType)
struct FPCGExSegmentLengthFilterConfig
{
	GENERATED_BODY()

	FPCGExSegmentLengthFilterConfig() = default;

	/** Whether to read the threshold from an attribute on the point or a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Attribute to fetch threshold from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold (Attr)", EditCondition="ThresholdInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", ClampMin=1, EditCondition="ThresholdInput == EPCGExInputValueType::Constant", EditConditionHides))
	double ThresholdConstant = 100;

	/** If enabled, will compare against the squared distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Squared Distance"))
	bool bCompareAgainstSquaredDistance = false;

	PCGEX_SETTING_VALUE_DECL(Threshold, double)

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0;


	/** Index mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexMode IndexMode = EPCGExIndexMode::Offset;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Index value to use according to the selected Index Mode -- Will be translated to `int32` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Index (Attr)", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector IndexAttribute;

	/** Const Index value to use according to the selected Index Mode, If offset mode, 1 would be next point, -1 previous point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Index", EditCondition="CompareAgainst == EPCGExInputValueType::Constant", EditConditionHides))
	int32 IndexConstant = 1;

	/** Index safety */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Clamp;

	/** If enabled, will force Tile safety on closed loop paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Tile on closed loops"))
	bool bForceTileIfClosedLoop = true;

	PCGEX_SETTING_VALUE_DECL(Index, int32)

	/** What should this filter return when the point required for computing length is invalid? (i.e, first or last point) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFilterFallback InvalidPointFallback = EPCGExFilterFallback::Fail;

	/** Whether the result of the filter should be inverted or not. Note that this will also invert fallback results! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bInvert = false;

	void Sanitize()
	{
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExSegmentLengthFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExSegmentLengthFilterConfig Config;

	virtual bool Init(FPCGExContext* InContext) override;

	virtual bool DomainCheck() override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual bool SupportsCollectionEvaluation() const override { return false; }
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointFilter
{
	class FSegmentLengthFilter final : public ISimpleFilter
	{
	public:
		explicit FSegmentLengthFilter(const TObjectPtr<const UPCGExSegmentLengthFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const TObjectPtr<const UPCGExSegmentLengthFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExDetails::TSettingValue<double>> Threshold;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> Index;
		bool bOffset = false;

		bool bClosedLoop = false;
		int32 LastIndex = -1;

		TConstPCGValueRange<FTransform> InTransforms;

		EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Clamp;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FSegmentLengthFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/self-comparisons/numeric-2"))
class UPCGExSegmentLengthFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SegmentLengthFilterFactory, "Filter : Segment Length", "Creates a filter definition that compares the distance between the tested point and another inside the same dataset.")
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSegmentLengthFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

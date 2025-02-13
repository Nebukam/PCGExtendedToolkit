// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExFilterGroup.h"


#include "Misc/Filters/PCGExFilterFactoryProvider.h"


#include "PCGExTagValueFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTagValueFilterConfig
{
	GENERATED_BODY()

	FPCGExTagValueFilterConfig()
	{
	}

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name", EditConditionHides, HideEditConditionToggle))
	FString Tag = TEXT("Tag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Match"))
	EPCGExStringMatchMode Match = EPCGExStringMatchMode::Equals;

	/** Expected value type, this is a strict check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExComparisonDataType ValueType = EPCGExComparisonDataType::Numeric;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Comparison", EditCondition="ValueType==EPCGExComparisonDataType::Numeric", EditConditionHides))
	EPCGExComparison NumericComparison = EPCGExComparison::NearlyEqual;

	/** Constant tag string value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B (Numeric)", EditCondition="ValueType==EPCGExComparisonDataType::Numeric", EditConditionHides, HideEditConditionToggle))
	double NumericOperandB = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ValueType==EPCGExComparisonDataType::Numeric && (NumericComparison==EPCGExComparison::NearlyEqual || NumericComparison==EPCGExComparison::NearlyNotEqual)", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Comparison", EditCondition="ValueType==EPCGExComparisonDataType::String", EditConditionHides))
	EPCGExStringComparison StringComparison = EPCGExStringComparison::Contains;

	/** Constant tag string value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B (String)", EditCondition="ValueType==EPCGExComparisonDataType::String", EditConditionHides, HideEditConditionToggle))
	FString StringOperandB = TEXT("Tag");

	/** OR only requires a single match to pass, AND requires all matches to pass. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFilterGroupMode MultiMatch = EPCGExFilterGroupMode::AND;

	/** Invert the result of this filter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bInvert = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTagValueFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTagValueFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExPointFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FTagValueFilter final : public FCollectionFilter
	{
	public:
		explicit FTagValueFilter(const TObjectPtr<const UPCGExTagValueFilterFactory>& InDefinition)
			: FCollectionFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExTagValueFilterFactory> TypedFilterFactory;

		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FTagValueFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTagValueFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		TagValueFilterFactory, "C-Filter : Tag Value", "Test the value of one or multiple tags",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTagValueFilterConfig Config;

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputColFilterLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};

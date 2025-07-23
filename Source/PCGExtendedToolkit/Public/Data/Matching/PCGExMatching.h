// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

#include "PCGExDetails.h"
#include "PCGExDetailsData.h"
#include "Data/PCGExData.h"

#include "PCGExCompare.h"
#include "Data/PCGExFilterGroup.h"

#include "PCGExMatching.generated.h"

UENUM()
enum class EPCGExMapMatchMode : uint8
{
	Disabled = 0 UMETA(DisplayName = "Disabled", ToolTip="Disabled"),
	All      = 1 UMETA(DisplayName = "All", ToolTip="All tests must pass to consider a match successful"),
	Any      = 2 UMETA(DisplayName = "Any", ToolTip="Any single test must pass must to consider a match successful"),
};

UENUM()
enum class EPCGExMatchStrictness : uint8
{
	Required = 0 UMETA(DisplayName = "Required", ToolTip="Required check. If it fails, the match will be a fail."),
	Any      = 1 UMETA(DisplayName = "Optional", ToolTip="Optional check. If it fails but other pass, it's still a success."),
};

/**
 * Base struct for match & compare utils
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMatchAndCompareDetails
{
	GENERATED_BODY()
	virtual ~FPCGExMatchAndCompareDetails() = default;

	FPCGExMatchAndCompareDetails()
	{
	}

	virtual bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade)
	PCGEX_NOT_IMPLEMENTED_RET(Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade), false);

	virtual bool Matches(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExData::FConstPoint& SourcePoint) const
	PCGEX_NOT_IMPLEMENTED_RET(Matches(const TSharedPtr<PCGExData::FTags>& InTags, const PCGExData::FConstPoint& SourcePoint), false);

	virtual void RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
	PCGEX_NOT_IMPLEMENTED(RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData));

	virtual bool GetOnlyUseDataDomain() const
	PCGEX_NOT_IMPLEMENTED_RET(GetOnlyUseDataDomain(), false);
};

/**
 * Used when individual points should be checked for a pick/match with data
 * The data will be tested for one or more tag, with optional value.
 * If value match is enabled, tag is expected to be in the format tag:value and value will be compared
 * against a point' attribute value.
 * Example :
 * Data 1 | Tag "MyTag", "MyTagValue:42"
 * Point 1 | "MyIntAttribute" = 42
 * Point 1 can be matched to MyTagValue using MyIntAttribute
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeToTagComparisonDetails : public FPCGExMatchAndCompareDetails
{
	GENERATED_BODY()

	FPCGExAttributeToTagComparisonDetails()
	{
	}

	/** Type of Tag Name value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType TagNameInput = EPCGExInputValueType::Constant;

	/** Attribute to read tag name value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name (Attr)", EditCondition="TagNameInput != EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FName TagNameAttribute = FName("Tag");

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name", EditCondition="TagNameInput == EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FString TagName = TEXT("Tag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Match"))
	EPCGExStringMatchMode NameMatch = EPCGExStringMatchMode::Equals;

	/** Whether to do a tag value match or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bDoValueMatch = false;

	/** Expected value type, this is a strict check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoValueMatch"))
	EPCGExComparisonDataType ValueType = EPCGExComparisonDataType::Numeric;

	/** Attribute to read tag name value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoValueMatch"))
	FPCGAttributePropertyInputSelector ValueAttribute;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Comparison", EditCondition="bDoValueMatch && ValueType == EPCGExComparisonDataType::Numeric", EditConditionHides))
	EPCGExComparison NumericComparison = EPCGExComparison::NearlyEqual;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bDoValueMatch && ValueType == EPCGExComparisonDataType::Numeric && (NumericComparison == EPCGExComparison::NearlyEqual || NumericComparison == EPCGExComparison::NearlyNotEqual)", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Comparison", EditCondition="bDoValueMatch && ValueType == EPCGExComparisonDataType::String", EditConditionHides))
	EPCGExStringComparison StringComparison = EPCGExStringComparison::Contains;

	TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> TagNameGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> NumericValueGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> StringValueGetter;

	virtual bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade) override;
	virtual bool Matches(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExData::FConstPoint& SourcePoint) const override;
	virtual void RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
	virtual bool GetOnlyUseDataDomain() const override;
};

/**
 * Used when individual points should be checked for a pick/match with data
 * A @Data attribute value will be compared against a point' value
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeToDataComparisonDetails	 : public FPCGExMatchAndCompareDetails
{
	GENERATED_BODY()

	FPCGExAttributeToDataComparisonDetails()
	{
	}

	/** Type of Data Name value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType DataNameInput = EPCGExInputValueType::Constant;

	/** Attribute to read data name value from. This attribute should contain the name of a @Data attribute to look for on matched data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Data Name (Attr)", EditCondition="DataNameInput != EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FName DataNameAttribute = FName("Key");

	/** Constant Data name value. This attribute must be present on matched data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Data Name", EditCondition="DataNameInput == EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FName DataName = TEXT("@Data.Value");

	/** Attribute to read data value from. This attribute value will be compared against the matched data' `@Data` attribute as defined above. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Value Name"))
	FName ValueNameAttribute = FName("Value");

	/** How should the data be compared. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComparisonDataType Check = EPCGExComparisonDataType::Numeric;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Comparison", EditCondition="Check == EPCGExComparisonDataType::Numeric", EditConditionHides))
	EPCGExComparison NumericCompare = EPCGExComparison::StrictlyEqual;

	/** Value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Value", EditCondition="Check == EPCGExComparisonDataType::Numeric", EditConditionHides))
	int64 NumericValue = 0;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Check == EPCGExComparisonDataType::Numeric && NumericCompare == EPCGExComparison::NearlyEqual || NumericCompare == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Comparison", EditCondition="Check == EPCGExComparisonDataType::String", EditConditionHides))
	EPCGExStringComparison StringCompare = EPCGExStringComparison::StrictlyEqual;

	/** Value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Value", EditCondition="Check == EPCGExComparisonDataType::String", EditConditionHides))
	FString StringValue = TEXT("");

	TSharedPtr<PCGEx::TAttributeBroadcaster<FName>> DataNameGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> NumericValueGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> StringValueGetter;

	virtual bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade) override;
	virtual bool Matches(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExData::FConstPoint& SourcePoint) const override;
	virtual void RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
	virtual bool GetOnlyUseDataDomain() const override;
};

/**
 * Used when data from different pins needs to be paired together
 * by using either tags or @Data attribute but no access to points.
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMatchingDetails
{
	GENERATED_BODY()

	virtual ~FPCGExMatchingDetails() = default;

	explicit FPCGExMatchingDetails(const EPCGExMapMatchMode InMode = EPCGExMapMatchMode::Disabled)
		: Mode(InMode)
	{
	}

	/** Whether matching is enabled or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExMapMatchMode Mode = EPCGExMapMatchMode::Disabled;

	/** Whether to output unmatched data in a separate pin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bSplitUnmatched = true;

	/** Whether to limit the number of matches or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Mode != EPCGExMapMatchMode::Disabled", EditConditionHides))
	bool bLimitMatches = false;

	/** Type of Match limit */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bLimitMatches && Mode != EPCGExMapMatchMode::Disabled", EditConditionHides))
	EPCGExInputValueType LimitInput = EPCGExInputValueType::Constant;

	/** Attribute to read Limit value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Limit (Attr)", EditCondition="bLimitMatches && LimitInput != EPCGExInputValueType::Constant && Mode != EPCGExMapMatchMode::Disabled", EditConditionHides))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Constant Limit value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Limit", EditCondition="bLimitMatches && LimitInput == EPCGExInputValueType::Constant && Mode != EPCGExMapMatchMode::Disabled", EditConditionHides))
	int32 Limit = 1;

	bool WantsUnmatchedSplit() const { return Mode != EPCGExMapMatchMode::Disabled && bSplitUnmatched; }
};

namespace PCGExMatching
{
	const FName OutputMatchRuleLabel = TEXT("Match Rule");
	const FName SourceMatchRulesLabel = TEXT("Match Rules");
	const FName OutputUnmatchedLabel = TEXT("Unmatched");

	PCGEXTENDEDTOOLKIT_API
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus = EPCGPinStatus::Normal);

	PCGEXTENDEDTOOLKIT_API
	void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);
}

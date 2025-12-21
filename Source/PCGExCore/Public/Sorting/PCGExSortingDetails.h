// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExSortingCommon.h"
#include "Details/PCGExAttributesDetails.h"

#include "PCGExSortingDetails.generated.h"

struct FPCGContext;

namespace PCGExData
{
	class FPointIOCollection;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExSortRuleConfig : public FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExSortRuleConfig() = default;
	FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other);


	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Invert sorting direction on that rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertRule = false;

	/** Compare absolute value. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//bool bAbsolute = false;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExCollectionSortingDetails
{
	GENERATED_BODY()

	FPCGExCollectionSortingDetails() = default;
	explicit FPCGExCollectionSortingDetails(const bool InEnabled);

	/** Whether this collection sorting is enabled or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bEnabled = false;

	/** Sorting direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Direction = EPCGExSortDirection::Ascending;

	/** Tag which value will be used for sorting; i.e MyTag:0, MyTag:1, MyTag:3 etc. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName TagName = FName("Tag");

	/** Multiplied applied to original order when tag is missing. Use -1/1 to choose whether these data should be put before or after the valid ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback", meta=(PCG_Overridable))
	double FallbackOrderOffset = 0;

	/** Multiplied applied to original order when tag is missing. Use -1/1 to choose whether these data should be put before or after the valid ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback", meta=(PCG_Overridable))
	double FallbackOrderMultiplier = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback")
	bool bQuietMissingTagWarning = false;

	bool Init(const FPCGContext* InContext);
	void Sort(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InCollection) const;
};

namespace PCGExSorting
{
	PCGEXCORE_API void DeclareSortingRulesInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus);

	PCGEXCORE_API TArray<FPCGExSortRuleConfig> GetSortingRules(FPCGExContext* InContext, const FName InLabel);
}

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES

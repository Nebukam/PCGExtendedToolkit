// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

#include "PCGExDetails.h"
#include "Data/PCGExData.h"
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

UENUM()
enum class EPCGExClusterComponentTagMatchMode : uint8
{
	Vtx       = 0 UMETA(DisplayName = "Vtx", ToolTip="Only match vtx (most efficient check)"),
	Edges     = 1 UMETA(DisplayName = "Edges", ToolTip="Only match edges"),
	Any       = 2 UMETA(DisplayName = "Any", ToolTip="Match either vtx or edges"),
	Both      = 3 UMETA(DisplayName = "Vtx and Edges", ToolTip="Match no vtx and edges"),
	Separated = 4 UMETA(DisplayName = "Separate", ToolTip="Uses two separate set of match handlers -- the default pin will be used on Vtx, the extra one for Edges."),
};

UENUM()
enum class EPCGExMatchingDetailsUsage : uint8
{
	Default  = 0,
	Cluster  = 1,
	Sampling = 2,
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

	explicit FPCGExMatchingDetails(const EPCGExMatchingDetailsUsage InUsage, const EPCGExMapMatchMode InMode = EPCGExMapMatchMode::Disabled)
		: Usage(InUsage), Mode(InMode)
	{
	}

	UPROPERTY()
	EPCGExMatchingDetailsUsage Usage = EPCGExMatchingDetailsUsage::Default;

	/** Whether matching is enabled or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExMapMatchMode Mode = EPCGExMapMatchMode::Disabled;

	/** Which cluster component must match the tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Usage == EPCGExMatchingDetailsUsage::Cluster", EditConditionHides, HideEditConditionToggle))
	EPCGExClusterComponentTagMatchMode ClusterMatchMode = EPCGExClusterComponentTagMatchMode::Vtx;

	/** Whether to output unmatched data in a separate pin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bSplitUnmatched = true;

	/** Whether to limit the number of matches or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Mode != EPCGExMapMatchMode::Disabled && Usage != EPCGExMatchingDetailsUsage::Sampling", EditConditionHides))
	bool bLimitMatches = false;

	/** Type of Match limit */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bLimitMatches && Mode != EPCGExMapMatchMode::Disabled && Usage != EPCGExMatchingDetailsUsage::Sampling", EditConditionHides))
	EPCGExInputValueType LimitInput = EPCGExInputValueType::Constant;

	/** Attribute to read Limit value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Limit (Attr)", EditCondition="bLimitMatches && LimitInput != EPCGExInputValueType::Constant && Mode != EPCGExMapMatchMode::Disabled && Usage != EPCGExMatchingDetailsUsage::Sampling", EditConditionHides))
	FPCGAttributePropertyInputSelector LimitAttribute;

	/** Constant Limit value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Limit", EditCondition="bLimitMatches && LimitInput == EPCGExInputValueType::Constant && Mode != EPCGExMapMatchMode::Disabled && Usage != EPCGExMatchingDetailsUsage::Sampling", EditConditionHides))
	int32 Limit = 1;

	bool WantsUnmatchedSplit() const { return Mode != EPCGExMapMatchMode::Disabled && bSplitUnmatched; }
};

namespace PCGExMatching
{
	const FName OutputMatchRuleLabel = TEXT("Match Rule");
	const FName SourceMatchRulesLabel = TEXT("Match Rules");
	const FName SourceMatchRulesEdgesLabel = TEXT("Match Rules (Edges)");
	const FName OutputUnmatchedLabel = TEXT("Unmatched");

	PCGEXTENDEDTOOLKIT_API
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	PCGEXTENDEDTOOLKIT_API
	void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);
}

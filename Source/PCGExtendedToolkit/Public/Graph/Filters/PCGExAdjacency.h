// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExSettings.h"
#include "Graph/PCGExGraph.h"

#include "PCGExAdjacency.generated.h"


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Test Mode"))
enum class EPCGExAdjacencyTestMode : uint8
{
	All UMETA(DisplayName = "All", Tooltip="Test a condition using all adjacent nodes."),
	Some UMETA(DisplayName = "Some", Tooltip="Test a condition using some adjacent nodes only.")
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Gather Mode"))
enum class EPCGExAdjacencyGatherMode : uint8
{
	Individual UMETA(DisplayName = "Individual", Tooltip="Test individual nodes"),
	Average UMETA(DisplayName = "Average", Tooltip="Average value"),
	Min UMETA(DisplayName = "Min", Tooltip="Min value"),
	Max UMETA(DisplayName = "Max", Tooltip="Max value"),
	Sum UMETA(DisplayName = "Sum", Tooltip="Sum value"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Subset Mode"))
enum class EPCGExAdjacencySubsetMode : uint8
{
	AtLeast UMETA(DisplayName = "At Least", Tooltip="Requirements must be met by at least X adjacent nodes."),
	AtMost UMETA(DisplayName = "At Most", Tooltip="Requirements must be met by at most X adjacent nodes."),
	Exactly UMETA(DisplayName = "Exactly", Tooltip="Requirements must be met by exactly X adjacent nodes, no more, no less.")
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Relative Rounding Mode"))
enum class EPCGExRelativeRoundingMode : uint8
{
	Round UMETA(DisplayName = "Round", Tooltip="Rounds value to closest integer (0.1 = 0, 0.9 = 1)"),
	Floor UMETA(DisplayName = "Floor", Tooltip="Rounds value to closest smaller integer (0.1 = 0, 0.9 = 0)"),
	Ceil UMETA(DisplayName = "Ceil", Tooltip="Rounds value to closest highest integer (0.1 = 1, 0.9 = 1)"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAdjacencySettings
{
	GENERATED_BODY()

	FPCGExAdjacencySettings()
	{
	}

	/** How many adjacent items should be tested. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAdjacencyTestMode Mode = EPCGExAdjacencyTestMode::All;

	/** How to consolidate value for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::All", EditConditionHides))
	EPCGExAdjacencyGatherMode Consolidation = EPCGExAdjacencyGatherMode::Average;

	/** How should adjacency be observed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExAdjacencySubsetMode SubsetMode = EPCGExAdjacencySubsetMode::AtLeast;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExMeanMeasure SubsetMeasure = EPCGExMeanMeasure::Absolute;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExFetchType SubsetSource = EPCGExFetchType::Constant;

	/** Local measure attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && SubsetSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalMeasure;

	/** Constant Local measure value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && SubsetSource==EPCGExFetchType::Constant", EditConditionHides))
	double ConstantMeasure = 0;

	bool IsRelativeMeasure() const { return SubsetMeasure == EPCGExMeanMeasure::Absolute; }
	bool IsLocalMeasure() const { return SubsetSource == EPCGExFetchType::Attribute; }
};

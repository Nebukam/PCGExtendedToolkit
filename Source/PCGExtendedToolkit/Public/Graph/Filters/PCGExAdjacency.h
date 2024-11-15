// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExDetails.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExAdjacency.generated.h"


UENUM()
enum class EPCGExAdjacencyTestMode : uint8
{
	All  = 0 UMETA(DisplayName = "All", Tooltip="Test a condition using all adjacent nodes."),
	Some = 1 UMETA(DisplayName = "Some", Tooltip="Test a condition using some adjacent nodes only.")
};

UENUM()
enum class EPCGExAdjacencyGatherMode : uint8
{
	Individual = 0 UMETA(DisplayName = "Individual", Tooltip="Test individual neighbors one by one"),
	Average    = 1 UMETA(DisplayName = "Average", Tooltip="Test against averaged value of all neighbors"),
	Min        = 2 UMETA(DisplayName = "Min", Tooltip="Test against Min value of all neighbors"),
	Max        = 3 UMETA(DisplayName = "Max", Tooltip="Test against Max value of all neighbors"),
	Sum        = 4 UMETA(DisplayName = "Sum", Tooltip="Test against Sum value of all neighbors"),
};

UENUM()
enum class EPCGExAdjacencyThreshold : uint8
{
	AtLeast = 0 UMETA(DisplayName = "At Least", Tooltip="Requirements must be met by at least N adjacent nodes.  (Where N is the Threshold)"),
	AtMost  = 1 UMETA(DisplayName = "At Most", Tooltip="Requirements must be met by at most N adjacent nodes.  (Where N is the Threshold)"),
	Exactly = 2 UMETA(DisplayName = "Exactly", Tooltip="Requirements must be met by exactly N adjacent nodes, no more, no less.  (Where N is the Threshold)")
};

UENUM()
enum class EPCGExRelativeThresholdRoundingMode : uint8
{
	Round = 0 UMETA(DisplayName = "Round", Tooltip="Rounds value to closest integer (0.1 = 0, 0.9 = 1)"),
	Floor = 1 UMETA(DisplayName = "Floor", Tooltip="Rounds value to closest smaller integer (0.1 = 0, 0.9 = 0)"),
	Ceil  = 2 UMETA(DisplayName = "Ceil", Tooltip="Rounds value to closest highest integer (0.1 = 1, 0.9 = 1)"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAdjacencySettings
{
	GENERATED_BODY()

	FPCGExAdjacencySettings()
	{
	}

	/** How many adjacent items should be tested. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAdjacencyTestMode Mode = EPCGExAdjacencyTestMode::Some;

	/** How to consolidate value for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExAdjacencyTestMode::All", EditConditionHides))
	EPCGExAdjacencyGatherMode Consolidation = EPCGExAdjacencyGatherMode::Average;

	/** How to handle threshold comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExComparison ThresholdComparison = EPCGExComparison::NearlyEqual;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExMeanMeasure ThresholdType = EPCGExMeanMeasure::Discrete;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, PCG_Overridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Discrete threshold value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", EditCondition="Mode==EPCGExAdjacencyTestMode::Some && ThresholdInput==EPCGExInputValueType::Constant && ThresholdType == EPCGExMeanMeasure::Discrete", EditConditionHides, ClampMin=0))
	int32 DiscreteThreshold = 1;

	/** Relative threshold value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", EditCondition="Mode==EPCGExAdjacencyTestMode::Some && ThresholdInput==EPCGExInputValueType::Constant && ThresholdType == EPCGExMeanMeasure::Relative", EditConditionHides, ClampMin=0, ClampMax=1))
	double RelativeThreshold = 0.5;

	/** Local measure attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some && ThresholdInput==EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** When using relative threshold mode, choose how to round it to a discrete value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some && ThresholdType == EPCGExMeanMeasure::Relative", EditConditionHides))
	EPCGExRelativeThresholdRoundingMode Rounding = EPCGExRelativeThresholdRoundingMode::Round;

	/** Comparison threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExAdjacencyTestMode::Some && (ThresholdComparison==EPCGExComparison::NearlyEqual || ThresholdComparison==EPCGExComparison::NearlyNotEqual)", EditConditionHides, ClampMin=0))
	int32 ThresholdTolerance = 0;

	bool bTestAllNeighbors = false;
	bool bUseDiscreteMeasure = false;
	bool bUseLocalThreshold = false;

	TSharedPtr<PCGExData::TBuffer<double>> LocalThreshold;

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPrimaryDataFacade)
	{
		bUseDiscreteMeasure = ThresholdType == EPCGExMeanMeasure::Discrete;
		bUseLocalThreshold = ThresholdInput == EPCGExInputValueType::Attribute;
		bTestAllNeighbors = Mode != EPCGExAdjacencyTestMode::Some;

		if (bUseLocalThreshold)
		{
			LocalThreshold = InPrimaryDataFacade->GetBroadcaster<double>(ThresholdAttribute);
			if (!LocalThreshold)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Threshold attribute: \"{0}\"."), FText::FromName(ThresholdAttribute.GetName())));
				return false;
			}
		}

		return true;
	}

	int32 GetThreshold(const PCGExCluster::FNode& Node) const
	{
		auto InternalEnsure = [&](const int32 Value)-> int32
		{
			switch (ThresholdComparison)
			{
			case EPCGExComparison::StrictlyEqual:
				return Node.Adjacency.Num() < Value ? -1 : Value;
			case EPCGExComparison::StrictlyNotEqual:
				return Value;
			case EPCGExComparison::EqualOrGreater:
				return Node.Adjacency.Num() < Value ? -1 : Value;
			case EPCGExComparison::EqualOrSmaller:
				return Value;
			case EPCGExComparison::StrictlyGreater:
				return Node.Adjacency.Num() <= Value ? -1 : Value;
			case EPCGExComparison::StrictlySmaller:
				return Value;
			case EPCGExComparison::NearlyEqual:
				return Value;
			case EPCGExComparison::NearlyNotEqual:
				return Value;
			default:
				return Value;
			}
		};

		if (bUseLocalThreshold)
		{
			if (bUseDiscreteMeasure)
			{
				// Fetch absolute subset count from node
				return InternalEnsure(LocalThreshold->Read(Node.PointIndex));
			}

			// Fetch relative subset count from node and factor the local adjacency count
			switch (Rounding)
			{
			default: ;
			case EPCGExRelativeThresholdRoundingMode::Round:
				return FMath::RoundToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Adjacency.Num());
			case EPCGExRelativeThresholdRoundingMode::Floor:
				return FMath::FloorToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Adjacency.Num());
			case EPCGExRelativeThresholdRoundingMode::Ceil:
				return FMath::CeilToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Adjacency.Num());
			}
		}

		if (bUseDiscreteMeasure)
		{
			// Use constant measure from settings
			return InternalEnsure(DiscreteThreshold);
		}

		switch (Rounding)
		{
		default: ;
		case EPCGExRelativeThresholdRoundingMode::Round:
			return FMath::RoundToInt32(RelativeThreshold * Node.Adjacency.Num());
		case EPCGExRelativeThresholdRoundingMode::Floor:
			return FMath::FloorToInt32(RelativeThreshold * Node.Adjacency.Num());
		case EPCGExRelativeThresholdRoundingMode::Ceil:
			return FMath::CeilToInt32(RelativeThreshold * Node.Adjacency.Num());
		}
	}
};

namespace PCGExAdjacency
{
}

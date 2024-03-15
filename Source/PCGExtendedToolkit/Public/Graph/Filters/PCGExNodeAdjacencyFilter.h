// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"
#include "Graph/PCGExCluster.h"
#include "..\PCGExCreateNodeFilter.h"

#include "PCGExNodeAdjacencyFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAdjacencyFilterDescriptor
{
	GENERATED_BODY()

	FPCGExAdjacencyFilterDescriptor()
	{
	}

	/** How should adjacency be observed. */
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
	EPCGExAdjacencySubsetMeasureMode SubsetMeasure = EPCGExAdjacencySubsetMeasureMode::AbsoluteStatic;

	/** Local measure attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && (SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::AbsoluteLocal||SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::RelativeLocal)", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalMeasure;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && (SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::RelativeStatic||SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::RelativeLocal)", EditConditionHides))
	EPCGExRelativeRoundingMode RoundingMode = EPCGExRelativeRoundingMode::Round;

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector OperandA;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Source of the Operand B value -- either the neighboring point, or the edge connecting to that point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, DisplayName="Operand B (Neighbor)"))
	FPCGAttributePropertyInputSelector OperandB;

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExAdjacencyFilterDefinition : public UPCGExClusterFilterDefinitionBase
{
	GENERATED_BODY()

public:
	EPCGExAdjacencyTestMode Mode = EPCGExAdjacencyTestMode::All;
	EPCGExAdjacencyGatherMode Consolidation = EPCGExAdjacencyGatherMode::Average;
	EPCGExAdjacencySubsetMode SubsetMode = EPCGExAdjacencySubsetMode::AtLeast;
	EPCGExAdjacencySubsetMeasureMode SubsetMeasure = EPCGExAdjacencySubsetMeasureMode::AbsoluteStatic;
	FPCGAttributePropertyInputSelector LocalMeasure;
	EPCGExRelativeRoundingMode RoundingMode = EPCGExRelativeRoundingMode::Round;
	FPCGAttributePropertyInputSelector OperandA;
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;
	FPCGAttributePropertyInputSelector OperandB;

	void ApplyDescriptor(const FPCGExAdjacencyFilterDescriptor& Descriptor)
	{
		Mode = Descriptor.Mode;
		Consolidation = Descriptor.Consolidation;
		SubsetMode = Descriptor.SubsetMode;
		SubsetMeasure = Descriptor.SubsetMeasure;
		LocalMeasure = Descriptor.LocalMeasure;
		RoundingMode = Descriptor.RoundingMode;
		OperandA = Descriptor.OperandA;
		Comparison = Descriptor.Comparison;
		OperandBSource = Descriptor.OperandBSource;
		OperandB = Descriptor.OperandB;
	}

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;
	virtual void BeginDestroy() override;
};

namespace PCGExNodeAdjacency
{
	class PCGEXTENDEDTOOLKIT_API TAdjacencyFilterHandler : public PCGExCluster::TClusterFilterHandler
	{
	public:
		explicit TAdjacencyFilterHandler(const UPCGExAdjacencyFilterDefinition* InDefinition)
			: TClusterFilterHandler(InDefinition), AdjacencyFilter(InDefinition)
		{
		}

		const UPCGExAdjacencyFilterDefinition* AdjacencyFilter;

		PCGEx::FLocalVectorGetter* OperandA = nullptr;
		PCGEx::FLocalVectorGetter* OperandB = nullptr;

		virtual void Capture(const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const PCGExData::FPointIO* EdgeIO) override;

		virtual bool Test(const int32 PointIndex) const override;
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeAdjacencyFilterSettings : public UPCGExCreateNodeFilterSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_TASKNAME(
		NodeAdjacencyFilter, "Cluster Filter : Adjacency", "Numeric comparison of adjacent values, testing either adjacent nodes or connected edges.",
		FName(FString(Descriptor.GetDisplayName())))

#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Test Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAdjacencyFilterDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExNodeAdjacencyFilterElement : public FPCGExCreateNodeFilterElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

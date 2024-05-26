// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"

#include "PCGExSettings.h"
#include "Data/PCGExGraphDefinition.h"
#include "Graph/PCGExCluster.h"
#include "../PCGExCreateNodeFilter.h"

#include "PCGExNodeAdjacencyFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAdjacencyFilterDescriptor
{
	GENERATED_BODY()

	FPCGExAdjacencyFilterDescriptor()
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && (SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::AbsoluteLocal||SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::RelativeLocal)", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalMeasure;

	/** Constant Local measure value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && (SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::AbsoluteStatic||SubsetMeasure==EPCGExAdjacencySubsetMeasureMode::RelativeStatic)", EditConditionHides))
	double ConstantMeasure = 0;

	/** Type of OperandA */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector OperandA;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	double OperandAConstant = 0;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Source of the Operand B value -- either the neighboring point, or the edge connecting to that point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, DisplayName="Operand B (Neighbor)"))
	FPCGAttributePropertyInputSelector OperandB;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;

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
	EPCGExMeanMeasure MeasureType = EPCGExMeanMeasure::Absolute;
	EPCGExFetchType MeasureSource = EPCGExFetchType::Constant;
	FPCGAttributePropertyInputSelector LocalMeasure;
	double ConstantMeasure;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;
	FPCGAttributePropertyInputSelector OperandA;
	double OperandAConstant = 0;
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;
	FPCGAttributePropertyInputSelector OperandB;
	double Tolerance = 0.001;

	void ApplyDescriptor(const FPCGExAdjacencyFilterDescriptor& Descriptor)
	{
		Mode = Descriptor.Mode;
		Consolidation = Descriptor.Consolidation;
		SubsetMode = Descriptor.SubsetMode;
		MeasureType = Descriptor.SubsetMeasure;
		MeasureSource = Descriptor.SubsetSource;
		LocalMeasure = Descriptor.LocalMeasure;
		ConstantMeasure = Descriptor.ConstantMeasure;
		CompareAgainst = Descriptor.CompareAgainst;
		OperandA = Descriptor.OperandA;
		OperandAConstant = Descriptor.OperandAConstant;
		Comparison = Descriptor.Comparison;
		OperandBSource = Descriptor.OperandBSource;
		OperandB = Descriptor.OperandB;
		Tolerance = Descriptor.Tolerance;
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

		TArray<double> CachedMeasure;

		bool bUseAbsoluteMeasure = false;
		bool bUseLocalMeasure = false;
		PCGEx::FLocalSingleFieldGetter* LocalMeasure = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandA = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		virtual void PrepareForTesting(PCGExData::FPointIO* PointIO) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TAdjacencyFilterHandler() override
		{
			PCGEX_DELETE(LocalMeasure)
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
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
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeAdjacencyFilter, "Cluster Filter : Adjacency", "Numeric comparison of adjacent values, testing either adjacent nodes or connected edges.",
		FName(Descriptor.GetDisplayName()))

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

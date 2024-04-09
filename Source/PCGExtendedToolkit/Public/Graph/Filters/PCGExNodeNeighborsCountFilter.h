// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"
#include "Graph/PCGExCluster.h"
#include "../PCGExCreateNodeFilter.h"

#include "PCGExNodeNeighborsCountFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNeighborsCountFilterDescriptor
{
	GENERATED_BODY()

	FPCGExNeighborsCountFilterDescriptor()
	{
	}

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of Count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector LocalCount;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	int32 Count = 0;

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
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborsCountFilterDefinition : public UPCGExClusterFilterDefinitionBase
{
	GENERATED_BODY()

public:
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;
	FPCGAttributePropertyInputSelector LocalCount;
	double Count = 0;
	double Tolerance = 0.001;

	void ApplyDescriptor(const FPCGExNeighborsCountFilterDescriptor& Descriptor)
	{
		CompareAgainst = Descriptor.CompareAgainst;
		LocalCount = Descriptor.LocalCount;
		Count = Descriptor.Count;
		Comparison = Descriptor.Comparison;
		Tolerance = Descriptor.Tolerance;
	}

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;
	virtual void BeginDestroy() override;
};

namespace PCGExNodeNeighborsCount
{
	class PCGEXTENDEDTOOLKIT_API TNeighborsCountFilterHandler : public PCGExCluster::TClusterFilterHandler
	{
	public:
		explicit TNeighborsCountFilterHandler(const UPCGExNeighborsCountFilterDefinition* InDefinition)
			: TClusterFilterHandler(InDefinition), NeighborsCountFilter(InDefinition)
		{
		}

		const UPCGExNeighborsCountFilterDefinition* NeighborsCountFilter;

		PCGEx::FLocalSingleFieldGetter* LocalCount = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TNeighborsCountFilterHandler() override
		{
			PCGEX_DELETE(LocalCount)
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeNeighborsCountFilterSettings : public UPCGExCreateNodeFilterSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeNeighborsCountFilter, "Cluster Filter : Neighbors Count", "Check against the node' neighbor count.",
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
	FPCGExNeighborsCountFilterDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExNodeNeighborsCountFilterElement : public FPCGExCreateNodeFilterElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

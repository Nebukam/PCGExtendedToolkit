// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExSettings.h"

#include "Data/PCGExGraphDefinition.h"
#include "Graph/PCGExCluster.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExAdjacencyFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAdjacencyFilterDescriptor
{
	GENERATED_BODY()

	FPCGExAdjacencyFilterDescriptor()
	{
	}

	/** Adjacency Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExAdjacencySettings Adjacency;

	/** Type of OperandA */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Attribute;

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector OperandA;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	double OperandAConstant = 0;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Source of the Operand B value -- either the neighboring point, or the edge connecting to that point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, DisplayName="Operand B (Neighbor)"))
	FPCGAttributePropertyInputSelector OperandB;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExAdjacencyFilterFactory : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExAdjacencyFilterDescriptor Descriptor;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeAdjacency
{
	class PCGEXTENDEDTOOLKIT_API TAdjacencyFilter final : public PCGExCluster::TClusterNodeFilter
	{
	public:
		explicit TAdjacencyFilter(const UPCGExAdjacencyFilterFactory* InFactory)
			: TClusterNodeFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Adjacency = InFactory->Descriptor.Adjacency;
		}

		const UPCGExAdjacencyFilterFactory* TypedFilterFactory;

		bool bCaptureFromNodes = false;
		
		TArray<double> CachedThreshold;
		FPCGExAdjacencySettings Adjacency;

		PCGEx::FLocalSingleFieldGetter* OperandA = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandB = nullptr;

		virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void CaptureCluster(const FPCGContext* InContext, const PCGExCluster::FCluster* InCluster) override;
		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		virtual void PrepareForTesting(const PCGExData::FPointIO* PointIO) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TAdjacencyFilter() override
		{
			Adjacency.Cleanup();
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExAdjacencyFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeAdjacencyFilterFactory, "Cluster Filter : Adjacency", "Numeric comparison of adjacent values, testing either adjacent nodes or connected edges.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

public:
	/** Test Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAdjacencyFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

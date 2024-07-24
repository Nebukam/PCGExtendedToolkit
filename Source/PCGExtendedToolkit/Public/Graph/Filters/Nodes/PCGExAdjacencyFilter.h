// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExDetails.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExAdjacencyFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAdjacencyFilterConfig
{
	GENERATED_BODY()

	FPCGExAdjacencyFilterConfig()
	{
	}

	/** Adjacency Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExAdjacencySettings Adjacency;

	/** Type of OperandA */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

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
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Vtx;

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
	FPCGExAdjacencyFilterConfig Config;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeAdjacency
{
	class PCGEXTENDEDTOOLKIT_API FAdjacencyFilter final : public PCGExClusterFilter::TFilter
	{
	public:
		explicit FAdjacencyFilter(const UPCGExAdjacencyFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Adjacency = InFactory->Config.Adjacency;
		}

		const UPCGExAdjacencyFilterFactory* TypedFilterFactory;

		bool bCaptureFromNodes = false;

		TArray<double> CachedThreshold;
		FPCGExAdjacencySettings Adjacency;

		PCGExData::FCache<double>* OperandA = nullptr;
		PCGExData::FCache<double>* OperandB = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade) override;

		virtual bool Test(const PCGExCluster::FNode& Node) const override;

		virtual ~FAdjacencyFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExAdjacencyFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeAdjacencyFilterFactory, "Cluster Filter : Adjacency", "Numeric comparison of adjacent values, testing either adjacent nodes or connected edges.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

public:
	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAdjacencyFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

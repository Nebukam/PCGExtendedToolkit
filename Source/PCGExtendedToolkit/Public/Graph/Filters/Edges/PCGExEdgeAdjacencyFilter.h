// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExDetails.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeAdjacencyFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeAdjacencyFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeAdjacencyFilterConfig()
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
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeAdjacencyFilterFactory : public UPCGExEdgeFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExEdgeAdjacencyFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FEdgeAdjacencyFilter final : public PCGExClusterFilter::TFilter
{
public:
	explicit FEdgeAdjacencyFilter(const UPCGExEdgeAdjacencyFilterFactory* InFactory)
		: TFilter(InFactory), TypedFilterFactory(InFactory)
	{
		Adjacency = InFactory->Config.Adjacency;
	}

	const UPCGExEdgeAdjacencyFilterFactory* TypedFilterFactory;

	bool bCaptureFromNodes = false;

	TArray<double> CachedThreshold;
	FPCGExAdjacencySettings Adjacency;

	TSharedPtr<PCGExData::TBuffer<double>> OperandA;
	TSharedPtr<PCGExData::TBuffer<double>> OperandB;

	using TestCallback = std::function<bool(const PCGExCluster::FNode&, const TArray<PCGExCluster::FNode>&, const double A)>;
	TestCallback TestSubFunc;

	virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;

	virtual bool Test(const PCGExCluster::FNode& Node) const override;

	virtual ~FEdgeAdjacencyFilter() override
	{
		TypedFilterFactory = nullptr;
	}
};


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeAdjacencyFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		EdgeAdjacencyFilterFactory, "Cluster Filter : Adjacency (Edge)", "Numeric comparison of endpoints values",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeAdjacencyFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExDetails.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExNodeAdjacencyFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExNodeAdjacencyFilterConfig
{
	GENERATED_BODY()

	FPCGExNodeAdjacencyFilterConfig()
	{
	}

	/** Adjacency Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExAdjacencySettings Adjacency;

	/** Type of OperandA */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExInputValueType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector OperandA;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	double OperandAConstant = 0;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Source of the Operand B value -- either the neighboring point, or the edge connecting to that point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterComponentSource OperandBSource = EPCGExClusterComponentSource::Vtx;

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
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNodeAdjacencyFilterFactory : public UPCGExNodeFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExNodeAdjacencyFilterConfig Config;

	virtual void GatherRequiredVtxAttributes(FPCGExContext* InContext, PCGExData::FReadableBufferConfigList& ReadableBufferConfigList) const override;
	
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FNodeAdjacencyFilter final : public PCGExClusterFilter::TNodeFilter
{
public:
	explicit FNodeAdjacencyFilter(const UPCGExNodeAdjacencyFilterFactory* InFactory)
		: TNodeFilter(InFactory), TypedFilterFactory(InFactory)
	{
		Adjacency = InFactory->Config.Adjacency;
	}

	const UPCGExNodeAdjacencyFilterFactory* TypedFilterFactory;

	bool bCaptureFromNodes = false;

	TArray<double> CachedThreshold;
	FPCGExAdjacencySettings Adjacency;

	TSharedPtr<PCGExData::TBuffer<double>> OperandA;
	TSharedPtr<PCGExData::TBuffer<double>> OperandB;

	using TestCallback = std::function<bool(const PCGExCluster::FNode&, const TArray<PCGExCluster::FNode>&, const double A)>;
	TestCallback TestSubFunc;

	virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;

	virtual bool Test(const PCGExCluster::FNode& Node) const override;

	virtual ~FNodeAdjacencyFilter() override
	{
		TypedFilterFactory = nullptr;
	}
};


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNodeAdjacencyFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeAdjacencyFilterFactory, "Cluster Filter : Adjacency (Node)", "Numeric comparison of adjacent values, testing either adjacent nodes or connected edges.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

	virtual FName GetMainOutputLabel() const override { return PCGExPointFilter::OutputFilterLabelNode; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNodeAdjacencyFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

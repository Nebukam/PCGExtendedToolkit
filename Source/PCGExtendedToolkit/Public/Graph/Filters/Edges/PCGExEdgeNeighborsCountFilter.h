// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeNeighborsCountFilter.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Refine Edge Threshold Mode")--E*/)
enum class EPCGExRefineEdgeThresholdMode : uint8
{
	Sum  = 0 UMETA(DisplayName = "Sum", Tooltip="The sum of adjacencies will be compared against the specified threshold"),
	Any  = 1 UMETA(DisplayName = "Any Endpoint", Tooltip="At least one endpoint adjacency count must pass the comparison against the specified threshold"),
	Both = 2 UMETA(DisplayName = "Both Endpoints", Tooltip="Both endpoint adjacency count must individually pass the comparison against the specified threshold"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeNeighborsCountFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeNeighborsCountFilterConfig()
	{
	}

	/** Whether to read the threshold from an attribute on the edge or a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** The number of connection endpoints must have to be considered a Bridge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="1", DisplayName="Threshold", EditCondition="ThresholdInput == EPCGExInputValueType::Constant", EditConditionHides))
	int32 ThresholdConstant = 2;

	/** Attribute to fetch threshold from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", EditCondition="ThresholdInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** How should we check if the threshold is reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeThresholdMode Mode = EPCGExRefineEdgeThresholdMode::Sum;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	int32 Tolerance = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeNeighborsCountFilterFactory : public UPCGExEdgeFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExEdgeNeighborsCountFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExEdgeNeighborsCount
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FNeighborsCountFilter final : public PCGExClusterFilter::TEdgeFilter
	{
	public:
		explicit FNeighborsCountFilter(const UPCGExEdgeNeighborsCountFilterFactory* InFactory)
			: TEdgeFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExEdgeNeighborsCountFilterFactory* TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<int32>> ThresholdBuffer;

		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override;

		virtual ~FNeighborsCountFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeNeighborsCountFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		EdgeNeighborsCountFilterFactory, "Cluster Filter : Neighbors Count (Edge)", "Check against the edge' endpoints neighbor count.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputLabel() const override { return PCGExPointFilter::OutputFilterLabelEdge; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeNeighborsCountFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGExCompare.h"
#include "Details/PCGExSettingsDetails.h"
#include "Core/PCGExClusterFilter.h"
#include "Core/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeNeighborsCountFilter.generated.h"

UENUM()
enum class EPCGExRefineEdgeThresholdMode : uint8
{
	Sum  = 0 UMETA(DisplayName = "Sum", Tooltip="The sum of adjacencies will be compared against the specified threshold"),
	Any  = 1 UMETA(DisplayName = "Any Endpoint", Tooltip="At least one endpoint adjacency count must pass the comparison against the specified threshold"),
	Both = 2 UMETA(DisplayName = "Both Endpoints", Tooltip="Both endpoint adjacency count must individually pass the comparison against the specified threshold"),
};

USTRUCT(BlueprintType)
struct FPCGExEdgeNeighborsCountFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeNeighborsCountFilterConfig() = default;

	/** Whether to read the threshold from an attribute on the edge or a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Attribute to fetch threshold from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold (Attr)", EditCondition="ThresholdInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** The number of connection endpoints must have to be considered a Bridge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", ClampMin=1, EditCondition="ThresholdInput == EPCGExInputValueType::Constant", EditConditionHides))
	int32 ThresholdConstant = 2;

	/** How should we check if the threshold is reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeThresholdMode Mode = EPCGExRefineEdgeThresholdMode::Sum;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	int32 Tolerance = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_SETTING_VALUE_DECL(Threshold, int32)
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExEdgeNeighborsCountFilterFactory : public UPCGExEdgeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExEdgeNeighborsCountFilterConfig Config;

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
};

namespace PCGExEdgeNeighborsCount
{
	class FFilter final : public PCGExClusterFilter::IEdgeFilter
	{
	public:
		explicit FFilter(const UPCGExEdgeNeighborsCountFilterFactory* InFactory)
			: IEdgeFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExEdgeNeighborsCountFilterFactory* TypedFilterFactory;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> ThresholdBuffer;

		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const override;

		virtual ~FFilter() override;
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="filters/filters-edges/neighbors-count"))
class UPCGExEdgeNeighborsCountFilterProviderSettings : public UPCGExEdgeFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(EdgeNeighborsCountFilterFactory, "Edge Filter : Neighbors Count", "Check against the edge' endpoints neighbor count.", PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(FilterCluster); }
#endif
	//~End UPCGSettings

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeNeighborsCountFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

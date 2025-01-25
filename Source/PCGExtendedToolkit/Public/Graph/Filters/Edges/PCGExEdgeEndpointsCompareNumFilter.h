// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeEndpointsCompareNumFilter.generated.h"

UENUM()
enum class EPCGExEdgeEndpointCompareAgainstMode : uint8
{
	AgainstEach     = 0 UMETA(DisplayName = "Start <-> End", Tooltip="Compare Edge's start point value against Edge's end point value."),
	AgainstStart    = 1 UMETA(DisplayName = "Edge <-> Start", Tooltip="Compare the Edge's start point value against the Edge itself."),
	AgainstEnd      = 2 UMETA(DisplayName = "Edge <-> End", Tooltip="Compare the Edge's end point value against the Edge itself."),
	AgainstSelfBoth = 3 UMETA(DisplayName = "Edge <-> Start, End", Tooltip="Compare the Edge's value against each of its end points."),
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeEndpointsCompareNumFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeEndpointsCompareNumFilterConfig()
	{
	}

	/** Attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Comparison"))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeEndpointsCompareNumFilterFactory : public UPCGExEdgeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExEdgeEndpointsCompareNumFilterConfig Config;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExEdgeEndpointsCompareNum
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FNeighborsCountFilter final : public PCGExClusterFilter::TEdgeFilter
	{
	public:
		explicit FNeighborsCountFilter(const UPCGExEdgeEndpointsCompareNumFilterFactory* InFactory)
			: TEdgeFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExEdgeEndpointsCompareNumFilterFactory* TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<double>> NumericBuffer;

		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override;

		virtual ~FNeighborsCountFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeEndpointsCompareNumFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		EdgeEndpointsCompareNumFilterFactory, "Cluster Filter : Endpoints Compare (Numeric)", "Compare the value of an attribute on each of the edge endpoint.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter); }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabelEdge; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeEndpointsCompareNumFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};

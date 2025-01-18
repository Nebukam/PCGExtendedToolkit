// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeEndpointsCompareStrFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeEndpointsCompareStrFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeEndpointsCompareStrFilterConfig()
	{
	}

	/** Attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Comparison"))
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeEndpointsCompareStrFilterFactory : public UPCGExEdgeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExEdgeEndpointsCompareStrFilterConfig Config;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExEdgeEndpointsCompareStr
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FNeighborsCountFilter final : public PCGExClusterFilter::TEdgeFilter
	{
	public:
		explicit FNeighborsCountFilter(const UPCGExEdgeEndpointsCompareStrFilterFactory* InFactory)
			: TEdgeFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExEdgeEndpointsCompareStrFilterFactory* TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<FString>> StringBuffer;

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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeEndpointsCompareStrFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		EdgeEndpointsCompareStrFilterFactory, "Cluster Filter : Endpoints Compare (String)", "Compare the value of an attribute on each of the edge endpoint.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabelEdge; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeEndpointsCompareStrFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};

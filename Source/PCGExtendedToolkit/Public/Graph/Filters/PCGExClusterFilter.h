// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointFilter.h"
#include "Graph/PCGExCluster.h"


#include "PCGExClusterFilter.generated.h"

namespace PCGExGraph
{
	struct FIndexedEdge;
}

namespace PCGExPointFilter
{
	class TFilter;
}

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterFilterFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNodeFilterFactoryBase : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterNode; }
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeFilterFactoryBase : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterEdge; }
};

namespace PCGExClusterFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TFilter : public PCGExPointFilter::TFilter
	{
	public:
		explicit TFilter(const TObjectPtr<const UPCGExClusterFilterFactoryBase>& InFactory):
			PCGExPointFilter::TFilter(InFactory)
		{
		}

		bool bInitForCluster = false;
		TSharedPtr<PCGExCluster::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		virtual PCGExFilters::EType GetFilterType() const override;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Init(const FPCGContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);
		virtual void PostInit() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TNodeFilter : public TFilter
	{
	public:
		explicit TNodeFilter(const TObjectPtr<const UPCGExClusterFilterFactoryBase>& InFactory):
			TFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Node; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override final;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TEdgeFilter : public TFilter
	{
	public:
		explicit TEdgeFilter(const TObjectPtr<const UPCGExClusterFilterFactoryBase>& InFactory):
			TFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Edge; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TManager : public PCGExPointFilter::TManager
	{
	public:
		TManager(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		bool bUseEdgeAsPrimary = false;
		TSharedRef<PCGExCluster::FCluster> Cluster;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		virtual ~TManager() override
		{
		}

	protected:
		virtual bool InitFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& Filter) override;
		virtual void InitCache() override;
	};
}

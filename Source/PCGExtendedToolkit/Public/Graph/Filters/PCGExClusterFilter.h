// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExClusterFilter.generated.h"

namespace PCGExGraph
{
	struct FIndexedEdge;
}

namespace PCGExCluster
{
	struct FNode;
	struct FCluster;
	struct FExpandedNode;
}

namespace PCGExPointFilter
{
	class TFilter;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterFilterFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeFilterFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeFilterFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
};

namespace PCGExClusterFilter
{
	class PCGEXTENDEDTOOLKIT_API TFilter : public PCGExPointFilter::TFilter
	{
	public:
		explicit TFilter(const UPCGExClusterFilterFactoryBase* InFactory):
			PCGExPointFilter::TFilter(InFactory)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExData::FPool* EdgeDataCache = nullptr;

		virtual PCGExFilters::EType GetFilterType() const override;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FPool* InPointDataCache) override;
		virtual bool Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FPool* InPointDataCache, PCGExData::FPool* InEdgeDataCache);
		virtual void PostInit() override;

	};

	class PCGEXTENDEDTOOLKIT_API TManager : public PCGExPointFilter::TManager
	{
	public:
		TManager(PCGExCluster::FCluster* InCluster, PCGExData::FPool* InPointDataCache, PCGExData::FPool* InEdgeDataCache);

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExData::FPool* EdgeDataCache = nullptr;

		virtual ~TManager() override
		{
		}

	protected:
		virtual bool InitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* Filter) override;

		virtual void InitCache() override;
	};
}
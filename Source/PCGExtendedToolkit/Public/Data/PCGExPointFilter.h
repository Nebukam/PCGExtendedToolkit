// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExFactoryProvider.h"


#include "PCGExPointFilter.generated.h"

namespace PCGExGraph
{
	struct FIndexedEdge;
}

namespace PCGExCluster
{
	struct FNode;
}

namespace PCGExPointFilter
{
	class TFilter;
}

namespace PCGExFilters
{
	enum class EType : uint8
	{
		None = 0,
		Point,
		Group,
		Node,
		ClusterEdge,
	};
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterPoint; }

	virtual bool Init(FPCGExContext* InContext);

	int32 Priority = 0;
	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const;
};

namespace PCGExPointFilter
{
	PCGEX_ASYNC_STATE(State_FilteringPoints)

	const FName OutputFilterLabel = FName("Filter");
	const FName SourceFiltersLabel = FName("Filters");

	const FName SourceFiltersConditionLabel = FName("Conditions Filters");
	const FName SourceKeepConditionLabel = FName("Keep Conditions");

	const FName SourcePointFiltersLabel = FName("Point Filters");
	const FName SourceVtxFiltersLabel = FName("Vtx Filters");
	const FName SourceEdgeFiltersLabel = FName("Edge Filters");

	const FName OutputInsideFiltersLabel = FName("Inside");
	const FName OutputOutsideFiltersLabel = FName("Outside");

	class /*PCGEXTENDEDTOOLKIT_API*/ TFilter
	{
	public:
		explicit TFilter(const TObjectPtr<const UPCGExFilterFactoryBase>& InFactory):
			Factory(InFactory)
		{
		}

		bool DefaultResult = true;
		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool bCacheResults = true;
		TObjectPtr<const UPCGExFilterFactoryBase> Factory;
		TArray<bool> Results;

		int32 FilterIndex = 0;

		virtual PCGExFilters::EType GetFilterType() const { return PCGExFilters::EType::Point; }

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade);

		virtual void PostInit();

		virtual bool Test(const int32 Index) const;
		virtual bool Test(const PCGExCluster::FNode& Node) const;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const;

		virtual ~TFilter() = default;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TManager
	{
	public:
		explicit TManager(const TSharedPtr<PCGExData::FFacade>& InPointDataFacade);

		bool bCacheResultsPerFilter = false;
		bool bCacheResults = false;
		TArray<bool> Results;

		bool bValid = false;

		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool Init(const FPCGContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories);

		virtual bool Test(const int32 Index);
		virtual bool Test(const PCGExCluster::FNode& Node);
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge);

		virtual ~TManager()
		{
		}

	protected:
		TArray<TSharedPtr<TFilter>> ManagedFilters;

		virtual bool InitFilter(const FPCGContext* InContext, const TSharedPtr<TFilter>& Filter);
		virtual bool PostInit(const FPCGContext* InContext);
		virtual void PostInitFilter(const FPCGContext* InContext, const TSharedPtr<TFilter>& InFilter);

		virtual void InitCache();
	};
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExFactoryProvider.h"

#include "Graph/PCGExCluster.h"

#include "PCGExPointFilter.generated.h"

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExPointFilter
{
	class FFilter;
}

namespace PCGExFilters
{
	enum class EType : uint8
	{
		None = 0,
		Point,
		Group,
		Node,
		Edge,
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
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
	{
	}
};

namespace PCGExPointFilter
{
	const FName OutputFilterLabel = FName("Filter");
	const FName OutputFilterLabelNode = FName("Node Filter");
	const FName OutputFilterLabelEdge = FName("Edge Filter");
	const FName SourceFiltersLabel = FName("Filters");

	const FName SourceFiltersConditionLabel = FName("Conditions Filters");
	const FName SourceKeepConditionLabel = FName("Keep Conditions");

	const FName SourcePointFiltersLabel = FName("Point Filters");
	const FName SourceVtxFiltersLabel = FName("Vtx Filters");
	const FName SourceEdgeFiltersLabel = FName("Edge Filters");

	const FName OutputInsideFiltersLabel = FName("Inside");
	const FName OutputOutsideFiltersLabel = FName("Outside");

	class /*PCGEXTENDEDTOOLKIT_API*/ FFilter
	{
	public:
		explicit FFilter(const TObjectPtr<const UPCGExFilterFactoryBase>& InFactory):
			Factory(InFactory)
		{
		}

		bool bUseEdgeAsPrimary = false; // This shouldn't be there but...
		
		bool DefaultResult = true;
		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool bCacheResults = true;
		TObjectPtr<const UPCGExFilterFactoryBase> Factory;
		TArray<int8> Results;

		int32 FilterIndex = 0;

		virtual PCGExFilters::EType GetFilterType() const { return PCGExFilters::EType::Point; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade);

		virtual void PostInit();

		virtual bool Test(const int32 Index) const;
		virtual bool Test(const PCGExCluster::FNode& Node) const;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const;

		virtual ~FFilter() = default;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FSimpleFilter : public FFilter
	{
	public:
		explicit FSimpleFilter(const TObjectPtr<const UPCGExFilterFactoryBase>& InFactory):
			FFilter(InFactory)
		{
		}

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override final;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FManager : public TSharedFromThis<FManager>
	{
	public:
		explicit FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		bool bUseEdgeAsPrimary = false; // This shouldn't be there...
		
		bool bCacheResultsPerFilter = false;
		bool bCacheResults = false;
		TArray<int8> Results;

		bool bValid = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories);

		virtual bool Test(const int32 Index);
		virtual bool Test(const PCGExCluster::FNode& Node);
		virtual bool Test(const PCGExGraph::FEdge& Edge);

		virtual ~FManager()
		{
		}

	protected:
		TArray<TSharedPtr<FFilter>> ManagedFilters;

		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<FFilter>& Filter);
		virtual bool PostInit(FPCGExContext* InContext);
		virtual void PostInitFilter(FPCGExContext* InContext, const TSharedPtr<FFilter>& InFilter);

		virtual void InitCache();
	};

	static void RegisterBuffersDependencies(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories, PCGExData::FFacadePreloader& FacadePreloader)
	{
		for (const UPCGExFilterFactoryBase* Factory : InFactories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}
}

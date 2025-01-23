// Copyright 2025 Timothé Lapetite and contributors
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterPoint; }
	virtual bool SupportsDirectEvaluation() const { return false; }

	virtual bool Init(FPCGExContext* InContext);

	int32 Priority = 0;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const;
};

namespace PCGExPointFilter
{
	const FName OutputFilterLabel = FName("Filter");
	const FName OutputFilterLabelNode = FName("Node Filter");
	const FName OutputFilterLabelEdge = FName("Edge Filter");
	const FName SourceFiltersLabel = FName("Filters");

	const FName SourceFiltersConditionLabel = FName("Conditions Filters");
	const FName SourceKeepConditionLabel = FName("Keep Conditions");
	const FName SourceStopConditionLabel = FName("Stop Conditions");

	const FName SourcePointFiltersLabel = FName("Point Filters");
	const FName SourceVtxFiltersLabel = FName("Vtx Filters");
	const FName SourceEdgeFiltersLabel = FName("Edge Filters");

	const FName OutputInsideFiltersLabel = FName("Inside");
	const FName OutputOutsideFiltersLabel = FName("Outside");

	class /*PCGEXTENDEDTOOLKIT_API*/ FFilter
	{
	public:
		explicit FFilter(const TObjectPtr<const UPCGExFilterFactoryData>& InFactory):
			Factory(InFactory)
		{
		}

		bool bUseEdgeAsPrimary = false; // This shouldn't be there but...

		bool DefaultResult = true;
		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool bCacheResults = true;
		TObjectPtr<const UPCGExFilterFactoryData> Factory;
		TArray<int8> Results;

		int32 FilterIndex = 0;

		virtual PCGExFilters::EType GetFilterType() const { return PCGExFilters::EType::Point; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade);

		virtual void PostInit();

		virtual bool Test(const int32 Index) const;
		virtual bool Test(const FPCGPoint& Point) const; // destined for live testing support only
		virtual bool Test(const PCGExCluster::FNode& Node) const;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const;

		virtual ~FFilter() = default;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FSimpleFilter : public FFilter
	{
	public:
		explicit FSimpleFilter(const TObjectPtr<const UPCGExFilterFactoryData>& InFactory):
			FFilter(InFactory)
		{
		}

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const FPCGPoint& Point) const override;
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

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories);

		virtual bool Test(const int32 Index);
		virtual bool Test(const FPCGPoint& Point);
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

	static void RegisterBuffersDependencies(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories, PCGExData::FFacadePreloader& FacadePreloader)
	{
		for (const UPCGExFilterFactoryData* Factory : InFactories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}

	static void PruneForDirectEvaluation(FPCGExContext* InContext, TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories)
	{
		if (InFactories.IsEmpty()) { return; }

		TArray<FString> UnsupportedFilters;
		UnsupportedFilters.Reserve(InFactories.Num());

		int32 WriteIndex = 0;
		for (int32 i = 0; i < InFactories.Num(); i++)
		{
			if (InFactories[i]->SupportsDirectEvaluation()) { InFactories[WriteIndex++] = InFactories[i]; }
			else { UnsupportedFilters.AddUnique(InFactories[i]->GetName()); }
		}

		InFactories.SetNum(WriteIndex);

		if (InFactories.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("None of the filters used supports direct evaluation."));
		}
		else if (!UnsupportedFilters.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some filters don't support direct evaluation and will be ignored: \"{0}\"."), FText::FromString(FString::Join(UnsupportedFilters, TEXT(", ")))));
		}
	}
}

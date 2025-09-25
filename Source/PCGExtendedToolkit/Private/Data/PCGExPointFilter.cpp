// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExCluster.h"

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoFilter, UPCGExFilterFactoryData)
PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoFilterCollection, UPCGExFilterCollectionFactoryData)

TSharedPtr<PCGExPointFilter::IFilter> UPCGExFilterFactoryData::CreateFilter() const
{
	return nullptr;
}

bool UPCGExFilterCollectionFactoryData::DomainCheck()
{
	return true;
}

bool UPCGExFilterCollectionFactoryData::SupportsCollectionEvaluation() const
{
	return true;
}


bool UPCGExFilterFactoryData::DomainCheck()
{
	return false;
}

bool UPCGExFilterFactoryData::Init(FPCGExContext* InContext)
{
	bOnlyUseDataDomain = DomainCheck(); // Will check selectors for @Data domain
	return true;
}

namespace PCGExPointFilter
{
	bool IFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		PointDataFacade = InPointDataFacade;
		return true;
	}

	void IFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}

	bool IFilter::Test(const int32 Index) const
	PCGEX_NOT_IMPLEMENTED_RET(FFilter::Test(const int32 Index), false)

	bool IFilter::Test(const PCGExData::FProxyPoint& Point) const
	PCGEX_NOT_IMPLEMENTED_RET(FFilter::Test(const PCGExData::FProxyPoint& Point), false)

	bool IFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool IFilter::Test(const PCGExGraph::FEdge& Edge) const { return Test(Edge.PointIndex); }

	bool IFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const { return bCollectionTestResult; }

	bool ISimpleFilter::Test(const int32 Index) const
	PCGEX_NOT_IMPLEMENTED_RET(FSimpleFilter::Test(const PCGExCluster::FNode& Node), false)

	bool ISimpleFilter::Test(const PCGExData::FProxyPoint& Point) const
	PCGEX_NOT_IMPLEMENTED_RET(FSimpleFilter::TestRoamingPoint(const PCGExCluster::PCGExData::FProxyPoint& Point), false)

	bool ISimpleFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool ISimpleFilter::Test(const PCGExGraph::FEdge& Edge) const { return Test(Edge.PointIndex); }

	bool ISimpleFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const { return bCollectionTestResult; }

	bool ICollectionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
		bCollectionTestResult = Test(InPointDataFacade->Source, nullptr);
		return true;
	}

	bool ICollectionFilter::Test(const int32 Index) const { return bCollectionTestResult; }
	bool ICollectionFilter::Test(const PCGExData::FProxyPoint& Point) const { return bCollectionTestResult; }

	bool ICollectionFilter::Test(const PCGExCluster::FNode& Node) const { return bCollectionTestResult; }
	bool ICollectionFilter::Test(const PCGExGraph::FEdge& Edge) const { return bCollectionTestResult; }

	bool ICollectionFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
	PCGEX_NOT_IMPLEMENTED_RET(FCollectionFilter::Test(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection), false)

	FManager::FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
	}

	bool FManager::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories)
	{
		for (const UPCGExFilterFactoryData* Factory : InFactories)
		{
			if (SupportedFactoriesTypes && !SupportedFactoriesTypes->Contains(Factory->GetFactoryType()))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("A filter is of an unexpected type : {0}."), PCGExHelpers::GetClassDisplayName(Factory->GetClass())));
				continue;
			}

			TSharedPtr<IFilter> NewFilter = Factory->CreateFilter();
			NewFilter->bUseDataDomainSelectorsOnly = Factory->GetOnlyUseDataDomain();
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			NewFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			NewFilter->SetSupportedTypes(SupportedFactoriesTypes);

			if (!InitFilter(InContext, NewFilter))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("A filter failed to initialize properly : {0}."), PCGExHelpers::GetClassDisplayName(Factory->GetClass())));
				continue;
			}
			ManagedFilters.Add(NewFilter);
		}

		return PostInit(InContext);
	}

	bool FManager::Test(const int32 Index)
	{
		for (const TSharedPtr<IFilter>& Handler : ManagedFilters) { if (!Handler->Test(Index)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExData::FProxyPoint& Point)
	{
		for (const TSharedPtr<IFilter>& Handler : ManagedFilters) { if (!Handler->Test(Point)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExCluster::FNode& Node)
	{
		for (const TSharedPtr<IFilter>& Handler : ManagedFilters) { if (!Handler->Test(Node)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExGraph::FEdge& Edge)
	{
		for (const TSharedPtr<IFilter>& Handler : ManagedFilters) { if (!Handler->Test(Edge)) { return false; } }
		return true;
	}

	bool FManager::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection)
	{
		for (const TSharedPtr<IFilter>& Handler : ManagedFilters) { if (!Handler->Test(IO, ParentCollection)) { return false; } }
		return true;
	}

	int32 FManager::Test(const PCGExMT::FScope Scope, TArray<int8>& OutResults)
	{
		bool bResult = true;
		int32 NumPass = 0;
		PCGEX_SCOPE_LOOP(Index)
		{
			bResult = true;
			for (const TSharedPtr<IFilter>& Handler : ManagedFilters)
			{
				if (!Handler->Test(Index))
				{
					bResult = false;
					break;
				}
			}
			OutResults[Index] = bResult;
			NumPass += bResult;
		}

		return NumPass;
	}

	int32 FManager::Test(const PCGExMT::FScope Scope, TBitArray<>& OutResults)
	{
		bool bResult = true;
		int32 NumPass = 0;
		PCGEX_SCOPE_LOOP(Index)
		{
			bResult = true;
			for (const TSharedPtr<IFilter>& Handler : ManagedFilters)
			{
				if (!Handler->Test(Index))
				{
					bResult = false;
					break;
				}
			}
			OutResults[Index] = bResult;
			NumPass += bResult;
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExCluster::FNode> Items, const TArrayView<int8> OutResults)
	{
		check(Items.Num() == OutResults.Num());

		bool bResult = true;
		int32 NumPass = 0;

		for (int i = 0; i < Items.Num(); i++)
		{
			bResult = true;
			const PCGExCluster::FNode& Node = Items[i];
			for (const TSharedPtr<IFilter>& Handler : ManagedFilters)
			{
				if (!Handler->Test(Node))
				{
					bResult = false;
					break;
				}
			}
			OutResults[i] = bResult;
			NumPass += bResult;
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExCluster::FNode> Items, const TSharedPtr<TArray<int8>>& OutResults)
	{
		bool bResult = true;
		int32 NumPass = 0;
		TArray<int8>& OutResultsRef = *OutResults.Get();

		for (int i = 0; i < Items.Num(); i++)
		{
			bResult = true;
			const PCGExCluster::FNode& Node = Items[i];
			for (const TSharedPtr<IFilter>& Handler : ManagedFilters)
			{
				if (!Handler->Test(Node))
				{
					bResult = false;
					break;
				}
			}

			OutResultsRef[Node.PointIndex] = bResult;
			NumPass += bResult;
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExGraph::FEdge> Items, const TArrayView<int8> OutResults)
	{
		check(Items.Num() == OutResults.Num());
		bool bResult = true;
		int32 NumPass = 0;

		for (int i = 0; i < Items.Num(); i++)
		{
			bResult = true;
			const PCGExGraph::FEdge& Edge = Items[i];
			for (const TSharedPtr<IFilter>& Handler : ManagedFilters)
			{
				if (!Handler->Test(Edge))
				{
					bResult = false;
					break;
				}
			}
			OutResults[i] = bResult;
			NumPass += bResult;
		}

		return NumPass;
	}

	void FManager::SetSupportedTypes(const TSet<PCGExFactories::EType>* InTypes)
	{
		SupportedFactoriesTypes = InTypes;
	}

	const TSet<PCGExFactories::EType>* FManager::GetSupportedTypes() const
	{
		return SupportedFactoriesTypes;
	}

	bool FManager::InitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& Filter)
	{
		return Filter->Init(InContext, PointDataFacade);
	}

	bool FManager::PostInit(FPCGExContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([](const TSharedPtr<IFilter>& A, const TSharedPtr<IFilter>& B) { return A->Factory->Priority < B->Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			TSharedPtr<IFilter> Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(InContext, Filter);
		}

		if (bCacheResults) { InitCache(); }

		return true;
	}

	void FManager::PostInitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& InFilter)
	{
		InFilter->PostInit();
	}

	void FManager::InitCache()
	{
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}
}

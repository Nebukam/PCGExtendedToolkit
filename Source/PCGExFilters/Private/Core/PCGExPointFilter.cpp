// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExPointFilter.h"

#include "PCGExFiltersSubSystem.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Async/ParallelFor.h"

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoFilter, UPCGExFilterFactoryData)
PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoFilterPoint, UPCGExPointFilterFactoryData)
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

	bool IFilter::Test(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(FFilter::Test(const int32 Index), false)

	bool IFilter::Test(const PCGExData::FProxyPoint& Point) const PCGEX_NOT_IMPLEMENTED_RET(FFilter::Test(const PCGExData::FProxyPoint& Point), false)

	bool IFilter::Test(const PCGExClusters::FNode& Node) const { return Test(Node.PointIndex); }
	bool IFilter::Test(const PCGExGraphs::FEdge& Edge) const { return Test(Edge.PointIndex); }

	bool IFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const { return bCollectionTestResult; }

	bool ISimpleFilter::Test(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(FSimpleFilter::Test(const PCGExClusters::FNode& Node), false)

	bool ISimpleFilter::Test(const PCGExData::FProxyPoint& Point) const PCGEX_NOT_IMPLEMENTED_RET(FSimpleFilter::TestRoamingPoint(const PCGExClusters::PCGExData::FProxyPoint& Point), false)

	bool ISimpleFilter::Test(const PCGExClusters::FNode& Node) const { return Test(Node.PointIndex); }
	bool ISimpleFilter::Test(const PCGExGraphs::FEdge& Edge) const { return Test(Edge.PointIndex); }

	bool ISimpleFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const { return bCollectionTestResult; }

	bool ICollectionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
		bCollectionTestResult = Test(InPointDataFacade->Source, nullptr);
		return true;
	}

	bool ICollectionFilter::Test(const int32 Index) const { return bCollectionTestResult; }
	bool ICollectionFilter::Test(const PCGExData::FProxyPoint& Point) const { return bCollectionTestResult; }

	bool ICollectionFilter::Test(const PCGExClusters::FNode& Node) const { return bCollectionTestResult; }
	bool ICollectionFilter::Test(const PCGExGraphs::FEdge& Edge) const { return bCollectionTestResult; }

	bool ICollectionFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const PCGEX_NOT_IMPLEMENTED_RET(FCollectionFilter::Test(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection), false)

	FManager::FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
	}

	bool FManager::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories)
	{
		bool bWantsTrueConstant = false;
		bool bWantsFalseConstant = false;

		for (const UPCGExPointFilterFactoryData* Factory : InFactories)
		{
			if (SupportedFactoriesTypes && !SupportedFactoriesTypes->Contains(Factory->GetFactoryType()))
			{
				PCGEX_LOG_INVALID_INPUT(InContext, FText::Format(FTEXT("A filter is of an unexpected type : {0}."), FText::FromString(GetNameSafe(Factory->GetClass()))))
				continue;
			}

			if (bWillBeUsedWithCollections && !Factory->SupportsCollectionEvaluation())
			{
				PCGEX_LOG_INVALID_INPUT(InContext, FText::Format(FTEXT("A filter can't be used with collections : {0}. (Requires per-point evaluation)"), FText::FromString(GetNameSafe(Factory->GetClass()))))
				continue;
			}

			TSharedPtr<IFilter> NewFilter = Factory->CreateFilter();
			NewFilter->bUseDataDomainSelectorsOnly = Factory->GetOnlyUseDataDomain();
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			NewFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			NewFilter->SetSupportedTypes(SupportedFactoriesTypes);
			NewFilter->bWillBeUsedWithCollections = bWillBeUsedWithCollections;

			if (!InitFilter(InContext, NewFilter))
			{
				if (Factory->InitializationFailurePolicy == EPCGExFilterNoDataFallback::Error)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("A filter failed to initialize properly : {0}."), FText::FromString(GetNameSafe(Factory->GetClass()))));
				}
				else if (Factory->InitializationFailurePolicy == EPCGExFilterNoDataFallback::Pass)
				{
					bWantsTrueConstant = true;
				}
				else
				{
					bWantsFalseConstant = true;
					break;
				}
				continue;
			}

			ManagedFilters.Add(NewFilter);
		}

		if (bWantsFalseConstant)
		{
			// Guaranteed fail.
			ManagedFilters.Reset();
			PCGEX_FILTERS_SUBSYSTEM
			const TSharedPtr<IFilter> NewFilter = PCGExFiltersSubsystem->GetConstantFilter(false);
			NewFilter->bUseDataDomainSelectorsOnly = true;
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			NewFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			NewFilter->bWillBeUsedWithCollections = bWillBeUsedWithCollections;

			InitFilter(InContext, NewFilter);

			ManagedFilters.Add(NewFilter);
		}
		else if (bWantsTrueConstant)
		{
			PCGEX_FILTERS_SUBSYSTEM
			const TSharedPtr<IFilter> NewFilter = PCGExFiltersSubsystem->GetConstantFilter(true);
			NewFilter->bUseDataDomainSelectorsOnly = true;
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			NewFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			NewFilter->bWillBeUsedWithCollections = bWillBeUsedWithCollections;

			InitFilter(InContext, NewFilter);

			if (ManagedFilters.IsEmpty()) { ManagedFilters.Add(NewFilter); }
			else { ManagedFilters.Insert(NewFilter, 0); }
		}

		return PostInit(InContext);
	}

	bool FManager::Test(const int32 Index)
	{
		for (const IFilter* Filter : Stack) { if (!Filter->Test(Index)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExData::FProxyPoint& Point)
	{
		for (const IFilter* Filter : Stack) { if (!Filter->Test(Point)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExClusters::FNode& Node)
	{
		for (const IFilter* Filter : Stack) { if (!Filter->Test(Node)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExGraphs::FEdge& Edge)
	{
		for (const IFilter* Filter : Stack) { if (!Filter->Test(Edge)) { return false; } }
		return true;
	}

	bool FManager::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection)
	{
		for (const IFilter* Filter : Stack) { if (!Filter->Test(IO, ParentCollection)) { return false; } }
		return true;
	}

#define PCGEX_TEST_STACK(_ITEM, _INDEX) bool bResult = true; for (const IFilter* Filter : Stack){if (!Filter->Test(_ITEM)){ bResult = false; break; }} OutResults[_INDEX] = bResult;

	int32 FManager::Test(const PCGExMT::FScope Scope, TArray<int8>& OutResults, const bool bParallel)
	{
		int32 NumPass = 0;

		if (bParallel)
		{
			ParallelFor(Scope.Count, [&](const int32 i)
			{
				const int32 Index = Scope.Start + i;
				PCGEX_TEST_STACK(Index, Index)
				if (bResult) { FPlatformAtomics::InterlockedIncrement(&NumPass); }
			});
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				PCGEX_TEST_STACK(Index, Index)
				NumPass += bResult;
			}
		}

		return NumPass;
	}

	int32 FManager::Test(const PCGExMT::FScope Scope, TBitArray<>& OutResults, const bool bParallel)
	{
		int32 NumPass = 0;

		if (bParallel)
		{
			ParallelFor(Scope.Count, [&](const int32 i)
			{
				const int32 Index = Scope.Start + i;
				PCGEX_TEST_STACK(Index, Index)
				if (bResult) { FPlatformAtomics::InterlockedIncrement(&NumPass); }
			});
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				PCGEX_TEST_STACK(Index, Index)
				NumPass += bResult;
			}
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExClusters::FNode> Items, const TArrayView<int8> OutResults, const bool bParallel)
	{
		const int32 NumItems = Items.Num();
		check(NumItems == OutResults.Num());

		int32 NumPass = 0;

		if (bParallel)
		{
			ParallelFor(NumItems, [&](const int32 i)
			{
				const PCGExClusters::FNode& Node = Items[i];
				PCGEX_TEST_STACK(Node, Node.PointIndex)
				if (bResult) { FPlatformAtomics::InterlockedIncrement(&NumPass); }
			});
		}
		else
		{
			for (int i = 0; i < NumItems; i++)
			{
				const PCGExClusters::FNode& Node = Items[i];
				PCGEX_TEST_STACK(Node, Node.PointIndex)
				NumPass += bResult;
			}
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExClusters::FNode> Items, const TSharedPtr<TArray<int8>>& OutResultsPtr, const bool bParallel)
	{
		int32 NumPass = 0;
		TArray<int8>& OutResults = *OutResultsPtr.Get();

		if (bParallel)
		{
			ParallelFor(Items.Num(), [&](const int32 i)
			{
				const PCGExClusters::FNode& Node = Items[i];
				PCGEX_TEST_STACK(Node, Node.PointIndex)
				if (bResult) { FPlatformAtomics::InterlockedIncrement(&NumPass); }
			});
		}
		else
		{
			for (const PCGExClusters::FNode& Node : Items)
			{
				PCGEX_TEST_STACK(Node, Node.PointIndex)
				NumPass += bResult;
			}
		}

		return NumPass;
	}

	int32 FManager::Test(const TArrayView<PCGExGraphs::FEdge> Items, const TArrayView<int8> OutResults, const bool bParallel)
	{
		const int32 NumItems = Items.Num();
		check(NumItems == OutResults.Num());
		int32 NumPass = 0;
		if (bParallel)
		{
			ParallelFor(NumItems, [&](const int32 i)
			{
				const PCGExGraphs::FEdge& Edge = Items[i];
				PCGEX_TEST_STACK(Edge, i)
				if (bResult) { FPlatformAtomics::InterlockedIncrement(&NumPass); }
			});
		}
		else
		{
			for (int i = 0; i < NumItems; i++)
			{
				const PCGExClusters::FEdge& Edge = Items[i];
				PCGEX_TEST_STACK(Edge, i)
				NumPass += bResult;
			}
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
		Stack.Reserve(ManagedFilters.Num());
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			TSharedPtr<IFilter> Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(InContext, Filter);
			Stack.Add(Filter.Get());
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


	void RegisterBuffersDependencies(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories, PCGExData::FFacadePreloader& FacadePreloader)
	{
		for (const UPCGExPointFilterFactoryData* Factory : InFactories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}

	void PruneForDirectEvaluation(FPCGExContext* InContext, TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories)
	{
		if (InFactories.IsEmpty()) { return; }

		TArray<FString> UnsupportedFilters;
		UnsupportedFilters.Reserve(InFactories.Num());

		int32 WriteIndex = 0;
		for (int32 i = 0; i < InFactories.Num(); i++)
		{
			if (InFactories[i]->SupportsProxyEvaluation()) { InFactories[WriteIndex++] = InFactories[i]; }
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

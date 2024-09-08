// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExFilterGroup.h"

#include "Graph/PCGExCluster.h"

namespace PCGExFilterGroup
{
	bool TFilterGroup::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
	{
		PointDataFacade = InPointDataFacade;
		return InitManaged(InContext);
	}

	bool TFilterGroup::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade)
	{
		bInitForCluster = true;
		Cluster = InCluster;
		PointDataFacade = InPointDataFacade;
		EdgeDataFacade = InEdgeDataFacade;
		return InitManaged(InContext);
	}

	void TFilterGroup::PostInit()
	{
		// No internal init yet, will cause issues with local caching

		/*
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataFacade->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
		*/
	}

	bool TFilterGroup::InitManaged(const FPCGContext* InContext)
	{
		for (const UPCGExFilterFactoryBase* ManagedFactory : *ManagedFactories)
		{
			PCGExPointFilter::TFilter* NewFilter = ManagedFactory->CreateFilter();
			NewFilter->bCacheResults = false;
			if (!InitManagedFilter(InContext, NewFilter))
			{
				delete NewFilter;
				continue;
			}
			ManagedFilters.Add(NewFilter);
		}

		return PostInitManaged(InContext);
	}

	bool TFilterGroup::InitManagedFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* Filter)
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Point) { return Filter->Init(InContext, PointDataFacade); }

		if (Filter->GetFilterType() == PCGExFilters::EType::Group)
		{
			if (bInitForCluster)
			{
				TFilterGroup* FilterGroup = static_cast<TFilterGroup*>(Filter);
				return FilterGroup->Init(InContext, Cluster, PointDataFacade, EdgeDataCache);
			}

			return Filter->Init(InContext, PointDataFacade);
		}

		if (Filter->GetFilterType() == PCGExFilters::EType::Node)
		{
			if (!bInitForCluster)
			{
				// Other filter types require cluster data, which we don't have :/
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Using a Cluster filter without cluster data"));
				return false;
			}

			TFilter* ClusterFilter = static_cast<TFilter*>(Filter);
			return ClusterFilter->Init(InContext, Cluster, PointDataFacade, EdgeDataCache);
		}

		return false;
	}

	bool TFilterGroup::PostInitManaged(const FPCGContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([&](const PCGExPointFilter::TFilter& A, const PCGExPointFilter::TFilter& B) { return A.Factory->Priority < B.Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			PCGExPointFilter::TFilter* Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitManagedFilter(InContext, Filter);
		}

		return true;
	}

	void TFilterGroup::PostInitManagedFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* InFilter)
	{
		InFilter->PostInit();
	}
}

PCGExPointFilter::TFilter* UPCGExFilterGroupFactoryBaseAND::CreateFilter() const
{
	PCGExFilterGroup::TFilterGroupAND* NewFilterGroup = new PCGExFilterGroup::TFilterGroupAND(this, &FilterFactories);
	NewFilterGroup->bInvert = bInvert;
	return NewFilterGroup;
}

PCGExPointFilter::TFilter* UPCGExFilterGroupFactoryBaseOR::CreateFilter() const
{
	PCGExFilterGroup::TFilterGroupOR* NewFilterGroup = new PCGExFilterGroup::TFilterGroupOR(this, &FilterFactories);
	NewFilterGroup->bInvert = bInvert;
	return NewFilterGroup;
}

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExFilterGroup.h"


#include "Graph/PCGExCluster.h"

namespace PCGExFilterGroup
{
	bool FFilterGroup::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		PointDataFacade = InPointDataFacade;
		return InitManaged(InContext);
	}

	bool FFilterGroup::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		bInitForCluster = true;
		Cluster = InCluster;
		PointDataFacade = InPointDataFacade;
		EdgeDataFacade = InEdgeDataFacade;
		return InitManaged(InContext);
	}

	void FFilterGroup::PostInit()
	{
		// No internal init yet, will cause issues with local caching

		/*
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataFacade->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
		*/
	}

	bool FFilterGroup::InitManaged(FPCGExContext* InContext)
	{
		for (const UPCGExFilterFactoryData* ManagedFactory : *ManagedFactories)
		{
			TSharedPtr<PCGExPointFilter::FFilter> NewFilter = ManagedFactory->CreateFilter();
			NewFilter->bCacheResults = false;
			if (!InitManagedFilter(InContext, NewFilter)) { continue; }
			ManagedFilters.Add(NewFilter);
		}

		return PostInitManaged(InContext);
	}

	bool FFilterGroup::InitManagedFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& Filter) const
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Group)
		{
			if (bInitForCluster)
			{
				FFilterGroup* FilterGroup = static_cast<FFilterGroup*>(Filter.Get());
				FilterGroup->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
				return FilterGroup->Init(InContext, Cluster.ToSharedRef(), PointDataFacade.ToSharedRef(), EdgeDataFacade.ToSharedRef());
			}

			return Filter->Init(InContext, PointDataFacade);
		}

		if (PCGExFactories::ClusterOnlyFilters.Contains(Filter->Factory->GetFactoryType()))
		{
			if (!bInitForCluster)
			{
				// Other filter types require cluster data, which we don't have :/
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Using a Cluster filter without cluster data"));
				return false;
			}

			FFilter* ClusterFilter = static_cast<FFilter*>(Filter.Get());
			ClusterFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			return ClusterFilter->Init(InContext, Cluster.ToSharedRef(), PointDataFacade.ToSharedRef(), EdgeDataFacade.ToSharedRef());
		}

		return Filter->Init(InContext, bUseEdgeAsPrimary ? EdgeDataFacade : PointDataFacade);
	}

	bool FFilterGroup::PostInitManaged(FPCGExContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([](const TSharedPtr<PCGExPointFilter::FFilter>& A, const TSharedPtr<PCGExPointFilter::FFilter>& B) { return A->Factory->Priority < B->Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			TSharedPtr<PCGExPointFilter::FFilter> Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitManagedFilter(InContext, Filter);
		}

		return true;
	}

	void FFilterGroup::PostInitManagedFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& InFilter)
	{
		InFilter->PostInit();
	}
}

bool UPCGExFilterGroupFactoryData::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	Super::RegisterConsumableAttributes(InContext);

	// Ensure we grab dependencies from plugged-in factories recursively
	for (const TObjectPtr<const UPCGExFilterFactoryData>& SubFilter : FilterFactories)
	{
		if (!SubFilter.Get()) { continue; }
		SubFilter->RegisterConsumableAttributes(InContext);
	}

	return true;
}

bool UPCGExFilterGroupFactoryData::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	Super::RegisterConsumableAttributesWithData(InContext, InData);

	// Ensure we grab dependencies from plugged-in factories recursively
	for (const TObjectPtr<const UPCGExFilterFactoryData>& SubFilter : FilterFactories)
	{
		if (!SubFilter.Get()) { continue; }
		SubFilter->RegisterConsumableAttributesWithData(InContext, InData);
	}

	return true;
}

void UPCGExFilterGroupFactoryData::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);

	// Ensure we grab dependencies from plugged-in factories recursively
	for (const TObjectPtr<const UPCGExFilterFactoryData>& SubFilter : FilterFactories)
	{
		if (!SubFilter.Get()) { continue; }
		SubFilter->RegisterAssetDependencies(InContext);
	}
}

void UPCGExFilterGroupFactoryData::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	// Ensure we grab dependencies from plugged-in factories recursively
	for (const TObjectPtr<const UPCGExFilterFactoryData>& SubFilter : FilterFactories)
	{
		if (!SubFilter.Get()) { continue; }
		SubFilter->RegisterBuffersDependencies(InContext, FacadePreloader);
	}
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExFilterGroupFactoryDataAND::CreateFilter() const
{
	PCGEX_MAKE_SHARED(NewFilterGroup, PCGExFilterGroup::FFilterGroupAND, this, &FilterFactories)
	NewFilterGroup->bInvert = bInvert;
	return NewFilterGroup;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExFilterGroupFactoryDataOR::CreateFilter() const
{
	PCGEX_MAKE_SHARED(NewFilterGroup, PCGExFilterGroup::FFilterGroupOR, this, &FilterFactories)
	NewFilterGroup->bInvert = bInvert;
	return NewFilterGroup;
}

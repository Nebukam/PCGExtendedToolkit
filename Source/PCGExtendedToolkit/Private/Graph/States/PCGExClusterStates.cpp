// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExClusterStates.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"

PCGExFactories::EType UPCGExClusterStateFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::StateNode;
}

PCGExPointFilter::TFilter* UPCGExClusterStateFactoryBase::CreateFilter() const
{
	PCGExClusterStates::FState* NewState = new PCGExClusterStates::FState(this);
	NewState->Descriptor = Descriptor;
	NewState->BaseDescriptor = &NewState->Descriptor;
	return NewState;
}

void UPCGExClusterStateFactoryBase::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExClusterStates
{
	FState::~FState()
	{
		PCGEX_DELETE(Manager)
	}

	bool FState::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataCache, PCGExData::FFacade* InEdgeDataCache)
	{
		Descriptor.Init();

		if (!TFilter::Init(InContext, InCluster, InPointDataCache, InEdgeDataCache)) { return false; }

		Manager = new PCGExClusterFilter::TManager(InCluster, PointDataCache, EdgeDataCache);
		Manager->bCacheResults = true;
		return true;
	}

	bool FState::InitInternalManager(const FPCGContext* InContext, const TArray<UPCGExFilterFactoryBase*>& InFactories)
	{
		return Manager->Init(InContext, InFactories);
	}

	bool FState::Test(const int32 Index) const
	{
		const bool bResult = Manager->Test(Index);
		Manager->Results[Index] = bResult;
		return bResult;
	}

	bool FState::Test(const PCGExCluster::FNode& Node) const
	{
		const bool bResult = Manager->Test(Node);
		Manager->Results[Node.NodeIndex] = bResult;
		return bResult;
	}

	bool FState::Test(const PCGExGraph::FIndexedEdge& Edge) const
	{
		const bool bResult = Manager->Test(Edge);
		Manager->Results[Edge.PointIndex] = bResult;
		return bResult;
	}

	void FState::ProcessFlags(const bool bSuccess, int64& InFlags) const
	{
		if (Descriptor.bOnTestPass && bSuccess) { Descriptor.PassStateFlags.DoOperation(InFlags); }
		else if (Descriptor.bOnTestFail && !bSuccess) { Descriptor.FailStateFlags.DoOperation(InFlags); }
	}

	FStateManager::FStateManager(TArray<int64>* InFlags, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataCache, PCGExData::FFacade* InEdgeDataCache)
		: TManager(InCluster, InPointDataCache, InEdgeDataCache)
	{
		FlagsCache = InFlags;
	}

	FStateManager::~FStateManager()
	{
		States.Empty();
	}

	bool FStateManager::Test(const int32 Index)
	{
		int64& Flags = (*FlagsCache)[Index];
		for (const FState* State : States) { State->ProcessFlags(State->Test(Index), Flags); }
		return true;
	}

	bool FStateManager::Test(const PCGExCluster::FNode& Node)
	{
		int64& Flags = (*FlagsCache)[Node.PointIndex];
		for (const FState* State : States) { State->ProcessFlags(State->Test(Node), Flags); }
		return true;
	}

	bool FStateManager::Test(const PCGExGraph::FIndexedEdge& Edge)
	{
		int64& Flags = (*FlagsCache)[Edge.PointIndex];
		for (const FState* State : States) { State->ProcessFlags(State->Test(Edge), Flags); }
		return true;
	}

	void FStateManager::PostInitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* InFilter)
	{
		FState* State = static_cast<FState*>(InFilter);
		State->InitInternalManager(InContext, State->StateFactory->FilterFactories);

		TManager::PostInitFilter(InContext, InFilter);

		States.Add(State);
	}
}


TArray<FPCGPinProperties> UPCGExClusterStateFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, TEXT("Filters uses to check whether this state is true or not"), Required, {})
	return PinProperties;
}

FName UPCGExClusterStateFactoryProviderSettings::GetMainOutputLabel() const { return PCGExCluster::OutputNodeStateLabel; }

UPCGExParamFactoryBase* UPCGExClusterStateFactoryProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExClusterStateFactoryBase* NewFactory = NewObject<UPCGExClusterStateFactoryBase>();
	NewFactory->Priority = Priority;
	NewFactory->Descriptor = Descriptor;

	if (!GetInputFactories(InContext, PCGExPointFilter::SourceFiltersLabel, NewFactory->FilterFactories, PCGExFactories::ClusterNodeFilters))
	{
		PCGEX_DELETE_UOBJECT(NewFactory)
		return nullptr;
	}

	return NewFactory;
}

FString UPCGExClusterStateFactoryProviderSettings::GetDisplayName() const
{
	return Name.ToString();
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointStates.h"

#include "Graph/PCGExCluster.h"

PCGExPointFilter::TFilter* UPCGExPointStateFactoryBase::CreateFilter() const
{
	return Super::CreateFilter();
}

void UPCGExPointStateFactoryBase::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExPointStates
{
	FState::~FState()
	{
		PCGEX_DELETE(Manager)
	}

	bool FState::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
	{
		if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

		Manager = new PCGExPointFilter::TManager(InPointDataFacade);
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

	void FState::ProcessFlags(const bool bSuccess, int64& InFlags) const
	{
		// TODO : Implement
	}

	FStateManager::FStateManager(TArray<int64>* InFlags, PCGExData::FFacade* InPointDataFacade)
		: TManager(InPointDataFacade)
	{
		FlagsCache = InFlags;
	}

	FStateManager::~FStateManager()
	{
		States.Empty();
	}

	void FStateManager::PostInitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* InFilter)
	{
		FState* State = static_cast<FState*>(InFilter);
		State->InitInternalManager(InContext, State->StateFactory->FilterFactories);

		TManager::PostInitFilter(InContext, InFilter);

		States.Add(State);
	}

	bool FStateManager::Test(const int32 Index)
	{
		int64& Flags = (*FlagsCache)[Index];
		for (const FState* State : States) { State->ProcessFlags(State->Test(Index), Flags); }
		return true;
	}
}

UPCGExParamFactoryBase* UPCGExPointStateFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return nullptr;
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointStates.h"


TSharedPtr<PCGExPointFilter::TFilter> UPCGExPointStateFactoryBase::CreateFilter() const
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
	}

	bool FState::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

		Manager = MakeUnique<PCGExPointFilter::TManager>(InPointDataFacade);
		Manager->bCacheResults = true;
		return true;
	}

	bool FState::InitInternalManager(const FPCGContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories)
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

	FStateManager::FStateManager(const TSharedPtr<TArray<int64>>& InFlags, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
		: TManager(InPointDataFacade)
	{
		FlagsCache = InFlags;
	}

	void FStateManager::PostInitFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& InFilter)
	{
		const TSharedPtr<FState> State = StaticCastSharedPtr<FState>(InFilter);
		State->InitInternalManager(InContext, State->StateFactory->FilterFactories);

		TManager::PostInitFilter(InContext, InFilter);

		States.Add(State);
	}

	bool FStateManager::Test(const int32 Index)
	{
		int64& Flags = (*FlagsCache)[Index];
		for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Index), Flags); }
		return true;
	}
}

UPCGExParamFactoryBase* UPCGExPointStateFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return nullptr;
}

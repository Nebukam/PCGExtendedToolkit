// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointStates.h"

#include "PCGExGlobalSettings.h"
#include "Graph/PCGExCluster.h"


TSharedPtr<PCGExPointFilter::IFilter> UPCGExPointStateFactoryData::CreateFilter() const
{
	return Super::CreateFilter();
}

void UPCGExPointStateFactoryData::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExPointStates
{
	FState::~FState()
	{
	}

	bool FState::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

		Manager = MakeShared<PCGExPointFilter::FManager>(InPointDataFacade.ToSharedRef());
		return true;
	}

	bool FState::InitInternalManager(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories)
	{
		return Manager->Init(InContext, InFactories);
	}

	bool FState::Test(const int32 Index) const
	{
		const bool bResult = Manager->Test(Index);
		return bResult;
	}

	void FState::ProcessFlags(const bool bSuccess, int64& InFlags) const
	{
		// TODO : Implement
	}

	FStateManager::FStateManager(const TSharedPtr<TArray<int64>>& InFlags, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: FManager(InPointDataFacade)
	{
		FlagsCache = InFlags;
	}

	void FStateManager::PostInitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& InFilter)
	{
		const TSharedPtr<FState> State = StaticCastSharedPtr<FState>(InFilter);
		State->InitInternalManager(InContext, State->StateFactory->FilterFactories);

		FManager::PostInitFilter(InContext, InFilter);

		States.Add(State);
	}

	bool FStateManager::Test(const int32 Index)
	{
		int64& Flags = (*FlagsCache)[Index];
		for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Index), Flags); }
		return true;
	}
}

#if WITH_EDITOR
FLinearColor UPCGExPointStateFactoryProviderSettings::GetNodeTitleColor() const { return GetDefault<UPCGExGlobalSettings>()->ColorClusterState; }
#endif

FName UPCGExPointStateFactoryProviderSettings::GetMainOutputPin() const { return PCGExCluster::OutputNodeFlagLabel; }


UPCGExFactoryData* UPCGExPointStateFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

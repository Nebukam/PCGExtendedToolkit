﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExClusterStates.h"


#include "PCGParamData.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoClusterState, UPCGExClusterStateFactoryData)

TSharedPtr<PCGExPointFilter::IFilter> UPCGExClusterStateFactoryData::CreateFilter() const
{
	PCGEX_MAKE_SHARED(NewState, PCGExClusterStates::FState, this)
	NewState->Config = Config;
	NewState->BaseConfig = &NewState->Config;
	return NewState;
}

void UPCGExClusterStateFactoryData::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	PCGExPointFilter::RegisterBuffersDependencies(InContext, FilterFactories, FacadePreloader);
}

void UPCGExClusterStateFactoryData::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExClusterStates
{
	FState::~FState()
	{
	}

	bool FState::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		Config.Init();

		if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		Manager = MakeShared<PCGExClusterFilter::FManager>(InCluster, PointDataFacade.ToSharedRef(), EdgeDataFacade.ToSharedRef());
		Manager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
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

	bool FState::Test(const PCGExCluster::FNode& Node) const
	{
		const bool bResult = Manager->Test(Node);
		return bResult;
	}

	bool FState::Test(const PCGExGraph::FEdge& Edge) const
	{
		const bool bResult = Manager->Test(Edge);
		return bResult;
	}

	void FState::ProcessFlags(const bool bSuccess, int64& InFlags) const
	{
		if (Config.bOnTestPass && bSuccess) { Config.PassStateFlags.DoOperation(InFlags); }
		else if (Config.bOnTestFail && !bSuccess) { Config.FailStateFlags.DoOperation(InFlags); }
	}

	FStateManager::FStateManager(
		const TSharedPtr<TArray<int64>>& InFlags,
		const TSharedRef<PCGExCluster::FCluster>& InCluster,
		const TSharedRef<PCGExData::FFacade>& InPointDataCache,
		const TSharedRef<PCGExData::FFacade>& InEdgeDataCache)
		: FManager(InCluster, InPointDataCache, InEdgeDataCache)
	{
		FlagsCache = InFlags;
	}

	void FStateManager::PostInitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& InFilter)
	{
		const TSharedPtr<FState>& State = StaticCastSharedPtr<FState>(InFilter);
		State->InitInternalManager(InContext, State->StateFactory->FilterFactories);

		FManager::PostInitFilter(InContext, InFilter);

		States.Add(State);
	}
}


TArray<FPCGPinProperties> UPCGExClusterStateFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FILTERS(PCGExPointFilter::SourceFiltersLabel, TEXT("Filters used to check whether this state is true or not. Accepts regular point filters & cluster filters."), Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExClusterStateFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (Config.bOnTestPass) { PCGEX_PIN_PARAMS(PCGExNodeFlags::OutputOnPassBitmaskLabel, TEXT("On Pass Bitmask. Note that based on the selected operation, this value may not be useful."), Advanced) }
	if (Config.bOnTestFail) { PCGEX_PIN_PARAMS(PCGExNodeFlags::OutputOnFailBitmaskLabel, TEXT("On Fail Bitmask. Note that based on the selected operation, this value may not be useful."), Advanced) }
	return PinProperties;
}

UPCGExFactoryData* UPCGExClusterStateFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExClusterStateFactoryData* NewFactory = InContext->ManagedObjects->New<UPCGExClusterStateFactoryData>();
	NewFactory->Priority = Priority;
	NewFactory->Config = Config;

	Super::CreateFactory(InContext, NewFactory);

	if (!GetInputFactories(
		InContext, PCGExPointFilter::SourceFiltersLabel, NewFactory->FilterFactories,
		PCGExFactories::ClusterNodeFilters))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	if (Config.bOnTestPass)
	{
		UPCGParamData* Bitmask = InContext->ManagedObjects->New<UPCGParamData>();
		Bitmask->Metadata->CreateAttribute<int64>(FName("OnPassBitmask"), Config.PassStateFlags.Get(), false, true);
		Bitmask->Metadata->AddEntry();
		FPCGTaggedData& OutData = InContext->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = PCGExNodeFlags::OutputOnPassBitmaskLabel;
		OutData.Data = Bitmask;
	}

	if (Config.bOnTestFail)
	{
		UPCGParamData* Bitmask = InContext->ManagedObjects->New<UPCGParamData>();
		Bitmask->Metadata->CreateAttribute<int64>(FName("OnFailBitmask"), Config.FailStateFlags.Get(), false, true);
		Bitmask->Metadata->AddEntry();
		FPCGTaggedData& OutData = InContext->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = PCGExNodeFlags::OutputOnFailBitmaskLabel;
		OutData.Data = Bitmask;
	}

	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExClusterStateFactoryProviderSettings::GetDisplayName() const
{
	return Name.ToString();
}
#endif

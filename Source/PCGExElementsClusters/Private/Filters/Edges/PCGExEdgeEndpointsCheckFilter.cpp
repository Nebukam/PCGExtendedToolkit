// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Edges/PCGExEdgeEndpointsCheckFilter.h"


#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExManagedObjects.h"
#include "Graphs/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeEndpointsCheckFilter"
#define PCGEX_NAMESPACE EdgeEndpointsCheckFilter

void UPCGExEdgeEndpointsCheckFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	for (const TObjectPtr<const UPCGExPointFilterFactoryData>& Factory : FilterFactories)
	{
		Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
	}
}

bool UPCGExEdgeEndpointsCheckFilterFactory::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	if (!Super::RegisterConsumableAttributes(InContext)) { return false; }

	for (const TObjectPtr<const UPCGExPointFilterFactoryData>& Factory : FilterFactories)
	{
		if (!Factory->RegisterConsumableAttributes(InContext)) { return false; }
	}

	return true;
}

bool UPCGExEdgeEndpointsCheckFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	for (const TObjectPtr<const UPCGExPointFilterFactoryData>& Factory : FilterFactories)
	{
		if (!Factory->RegisterConsumableAttributesWithData(InContext, InData)) { return false; }
	}

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExEdgeEndpointsCheckFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeEndpointsCheck::FFilter>(this);
}

namespace PCGExEdgeEndpointsCheck
{
	bool FFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		VtxFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), InPointDataFacade, InEdgeDataFacade);
		VtxFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
		if (!VtxFiltersManager->Init(InContext, TypedFilterFactory->FilterFactories)) { return false; }

		ResultCache.Init(-1, Cluster->Nodes->Num());

		Expected = TypedFilterFactory->Config.Expects == EPCGExFilterResult::Fail ? 0 : 1;
		return true;
	}

	bool FFilter::Test(const PCGExGraphs::FEdge& Edge) const
	{
		TArray<int8>& MutableResultCache = const_cast<TArray<int8>&>(ResultCache);

		const PCGExClusters::FNode* Start = Cluster->GetEdgeStart(Edge);
		int8 StartResult = ResultCache[Start->Index]; // TODO Atomic read?

		const PCGExClusters::FNode* End = Cluster->GetEdgeEnd(Edge);
		int8 EndResult = ResultCache[End->Index];

		if (StartResult == -1)
		{
			StartResult = VtxFiltersManager->Test(*Start);
			FPlatformAtomics::AtomicStore(&MutableResultCache[Start->Index], StartResult);
		}

		if (EndResult == -1)
		{
			EndResult = VtxFiltersManager->Test(*End);
			FPlatformAtomics::AtomicStore(&MutableResultCache[End->Index], EndResult);
		}

		bool bPass = true;
		switch (TypedFilterFactory->Config.Mode)
		{
		case EPCGExEdgeEndpointsCheckMode::None: bPass = StartResult != Expected && EndResult != Expected;
			break;
		case EPCGExEdgeEndpointsCheckMode::Both: bPass = StartResult == Expected && EndResult == Expected;
			break;
		case EPCGExEdgeEndpointsCheckMode::Any: bPass = StartResult == Expected || EndResult == Expected;
			break;
		case EPCGExEdgeEndpointsCheckMode::Start: bPass = StartResult == Expected;
			break;
		case EPCGExEdgeEndpointsCheckMode::End: bPass = EndResult == Expected;
			break;
		case EPCGExEdgeEndpointsCheckMode::SeeSaw: bPass = StartResult != EndResult;
			break;
		}

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	FFilter::~FFilter()
	{
		VtxFiltersManager.Reset();
		TypedFilterFactory = nullptr;
	}
}

TArray<FPCGPinProperties> UPCGExEdgeEndpointsCheckFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FILTERS(PCGExFilters::Labels::SourceVtxFiltersLabel, TEXT("Filters used on endpoints."), Required)
	return PinProperties;
}

UPCGExFactoryData* UPCGExEdgeEndpointsCheckFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExEdgeEndpointsCheckFilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGExEdgeEndpointsCheckFilterFactory>();

	NewFactory->Config = Config;

	Super::CreateFactory(InContext, NewFactory);

	if (!GetInputFactories(InContext, PCGExFilters::Labels::SourceVtxFiltersLabel, NewFactory->FilterFactories, PCGExFactories::ClusterNodeFilters))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	if (!NewFactory->Init(InContext))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExEdgeEndpointsCheckFilterProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

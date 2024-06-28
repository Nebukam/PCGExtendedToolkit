// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Extras/PCGExVtxExtraFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExWriteVtxExtras"
#define PCGEX_NAMESPACE PCGExWriteVtxExtras


void UPCGExVtxExtraOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxExtraOperation* TypedOther = Cast<UPCGExVtxExtraOperation>(Other);
	if (TypedOther)
	{
	}
}

bool UPCGExVtxExtraOperation::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataCache)
{
	PrimaryDataCache = InVtxDataCache;
	SecondaryDataCache = nullptr;
	return true;
}

void UPCGExVtxExtraOperation::ClusterReserve(const int32 NumClusters)
{
}

void UPCGExVtxExtraOperation::PrepareForCluster(const FPCGContext* InContext, const int32 ClusterIdx, PCGExCluster::FCluster* Cluster, PCGExData::FFacade* VtxDataCache, PCGExData::FFacade* EdgeDataCache)
{
	PrimaryDataCache = VtxDataCache;
	SecondaryDataCache = EdgeDataCache;
}

bool UPCGExVtxExtraOperation::IsOperationValid() { return bIsValidOperation; }

void UPCGExVtxExtraOperation::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
}

void UPCGExVtxExtraOperation::Cleanup() { Super::Cleanup(); }

#if WITH_EDITOR
FString UPCGExVtxExtraProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGExFactories::EType UPCGExVtxExtraFactoryBase::GetFactoryType() const { return PCGExFactories::EType::VtxExtra; }

UPCGExVtxExtraOperation* UPCGExVtxExtraFactoryBase::CreateOperation() const
{
	UPCGExVtxExtraOperation* NewOperation = NewObject<UPCGExVtxExtraOperation>();
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxExtraProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	//PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	//PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

FName UPCGExVtxExtraProviderSettings::GetMainOutputLabel() const { return PCGExVtxExtra::OutputExtraLabel; }

UPCGExParamFactoryBase* UPCGExVtxExtraProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	//UPCGExVtxExtraFactoryBase* NewFactory = Cast<UPCGExVtxExtraFactoryBase>(InFactory);
	//SamplerFactory->Priority = Priority;
	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

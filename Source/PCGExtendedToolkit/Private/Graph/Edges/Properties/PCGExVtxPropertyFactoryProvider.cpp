// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExWriteVtxProperties"
#define PCGEX_NAMESPACE PCGExWriteVtxProperties


void UPCGExVtxPropertyOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExVtxPropertyOperation* TypedOther = Cast<UPCGExVtxPropertyOperation>(Other))
	{
	}
}

bool UPCGExVtxPropertyOperation::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade)
{
	PrimaryDataFacade = InVtxDataFacade;
	SecondaryDataFacade = nullptr;
	return true;
}

void UPCGExVtxPropertyOperation::ClusterReserve(const int32 NumClusters)
{
}

void UPCGExVtxPropertyOperation::PrepareForCluster(const FPCGContext* InContext, const int32 ClusterIdx, PCGExCluster::FCluster* Cluster, PCGExData::FFacade* VtxDataFacade, PCGExData::FFacade* EdgeDataFacade)
{
	PrimaryDataFacade = VtxDataFacade;
	SecondaryDataFacade = EdgeDataFacade;
}

bool UPCGExVtxPropertyOperation::IsOperationValid() { return bIsValidOperation; }

void UPCGExVtxPropertyOperation::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
}

void UPCGExVtxPropertyOperation::Cleanup() { Super::Cleanup(); }

#if WITH_EDITOR
FString UPCGExVtxPropertyProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertyFactoryBase::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExVtxPropertyOperation, NewOperation)
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxPropertyProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	//PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	//PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExVtxPropertyProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	//UPCGExVtxPropertyFactoryBase* NewFactory = Cast<UPCGExVtxPropertyFactoryBase>(InFactory);
	//SamplerFactory->Priority = Priority;
	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

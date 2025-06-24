// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"
#include "PCGPin.h"


#define LOCTEXT_NAMESPACE "PCGExWriteVtxProperties"
#define PCGEX_NAMESPACE PCGExWriteVtxProperties

bool FPCGExVtxPropertyOperation::PrepareForCluster(FPCGExContext* InContext, TSharedPtr<PCGExCluster::FCluster> InCluster, const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade)
{
	PrimaryDataFacade = InVtxDataFacade;
	SecondaryDataFacade = InEdgeDataFacade;
	Cluster = InCluster.Get();
	bIsValidOperation = true;
	return bIsValidOperation;
}

bool FPCGExVtxPropertyOperation::IsOperationValid() { return bIsValidOperation; }

void FPCGExVtxPropertyOperation::ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
}

#if WITH_EDITOR
FString UPCGExVtxPropertyProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

TSharedPtr<FPCGExVtxPropertyOperation> UPCGExVtxPropertyFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(VtxPropertyOperation)
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxPropertyProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	//PCGEX_PIN_FACTORIES(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	//PCGEX_PIN_FACTORIES(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

UPCGExFactoryData* UPCGExVtxPropertyProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

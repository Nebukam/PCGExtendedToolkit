// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExWriteVtxProperties"
#define PCGEX_NAMESPACE PCGExWriteVtxProperties


void UPCGExVtxPropertyOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	// if (const UPCGExVtxPropertyOperation* TypedOther = Cast<UPCGExVtxPropertyOperation>(Other))	{	}
}

bool UPCGExVtxPropertyOperation::PrepareForCluster(const FPCGContext* InContext, TSharedPtr<PCGExCluster::FCluster> InCluster, const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade)
{
	PrimaryDataFacade = InVtxDataFacade;
	SecondaryDataFacade = InEdgeDataFacade;
	Cluster = InCluster.Get();
	bIsValidOperation = true;
	return bIsValidOperation;
}

bool UPCGExVtxPropertyOperation::IsOperationValid() { return bIsValidOperation; }

void UPCGExVtxPropertyOperation::ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
}

void UPCGExVtxPropertyOperation::Cleanup() { Super::Cleanup(); }

#if WITH_EDITOR
FString UPCGExVtxPropertyProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertyFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExVtxPropertyOperation* NewOperation = InContext->ManagedObjects->New<UPCGExVtxPropertyOperation>();
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

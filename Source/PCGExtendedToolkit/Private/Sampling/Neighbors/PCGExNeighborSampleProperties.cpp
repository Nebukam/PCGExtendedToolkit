// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleProperties.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleProperties::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForCluster(InCluster);
}

void UPCGExNeighborSampleProperties::PrepareNode(PCGExCluster::FNode& TargetNode) const
{
	Super::PrepareNode(TargetNode);
}

void UPCGExNeighborSampleProperties::BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const
{
	Super::BlendNodePoint(TargetNode, OtherNode, Weight);
}

void UPCGExNeighborSampleProperties::BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const
{
	Super::BlendNodeEdge(TargetNode, InEdgeIndex, Weight);
}

void UPCGExNeighborSampleProperties::FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
{
	Super::FinalizeNode(TargetNode, Count, TotalWeight);
}

void UPCGExNeighborSampleProperties::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSamplePropertiesSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExNeighborSampleOperation* UPCGNeighborSamplerFactoryProperties::CreateOperation() const
{
	UPCGExNeighborSampleProperties* NewOperation = NewObject<UPCGExNeighborSampleProperties>();

	PCGEX_SAMPLER_CREATE
	
	NewOperation->BlendingSettings = Descriptor.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{

	UPCGNeighborSamplerFactoryProperties* SamplerFactory = Cast<UPCGNeighborSamplerFactoryProperties>(InFactory);
	SamplerFactory->Descriptor = Descriptor;
	
	Super::CreateFactory(InContext, SamplerFactory);
	
	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

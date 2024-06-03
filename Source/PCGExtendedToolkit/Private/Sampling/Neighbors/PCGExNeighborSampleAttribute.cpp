// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleAttribute::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForCluster(InCluster);
	for (FName AId : SourceAttributes)
	{
		if (PCGExDataBlending::FDataBlendingOperationBase** BlendOp = Blender->OperationIdMap.Find(AId)) { BlendOps.Add(*BlendOp); }
	}
}

void UPCGExNeighborSampleAttribute::PrepareNode(PCGExCluster::FNode& TargetNode) const
{
	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->PrepareOperation(TargetNode.PointIndex);
	}
}

void UPCGExNeighborSampleAttribute::BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const
{
	const int32 PrimaryIndex = TargetNode.PointIndex;
	const int32 SecondaryIndex = OtherNode.PointIndex;
	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->DoOperation(PrimaryIndex, SecondaryIndex, PrimaryIndex, Weight);
	}
}

void UPCGExNeighborSampleAttribute::BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const
{
	const int32 PrimaryIndex = TargetNode.PointIndex;
	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->DoOperation(PrimaryIndex, InEdgeIndex, PrimaryIndex, Weight);
	}
}

void UPCGExNeighborSampleAttribute::FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
{
	const int32 PrimaryIndex = TargetNode.PointIndex;
	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->FinalizeOperation(PrimaryIndex, Count, TotalWeight);
	}
}

void UPCGExNeighborSampleAttribute::Cleanup()
{
	BlendOps.Empty();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSampleAttributeSettings::GetDisplayName() const
{
	if (Descriptor.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = Descriptor.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
}
#endif

UPCGExNeighborSampleOperation* UPCGNeighborSamplerFactoryAttribute::CreateOperation() const
{
	UPCGExNeighborSampleAttribute* NewOperation = NewObject<UPCGExNeighborSampleAttribute>();

	PCGEX_SAMPLER_CREATE
	
	NewOperation->SourceAttributes = Descriptor.SourceAttributes;
	NewOperation->Blending = Descriptor.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSampleAttributeSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGNeighborSamplerFactoryAttribute* SamplerFactory = Cast<UPCGNeighborSamplerFactoryAttribute>(InFactory);
	SamplerFactory->Descriptor = Descriptor;
	
	Super::CreateFactory(InContext, SamplerFactory);
	
	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleProperties.h"

#include "Data/Blending/PCGExPropertiesBlender.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleProperties::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExNeighborSampleProperties* TypedOther = Cast<UPCGExNeighborSampleProperties>(Other);
	if (TypedOther)
	{
		BlendingSettings = TypedOther->BlendingSettings;
	}
}

void UPCGExNeighborSampleProperties::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade)
{
	PCGEX_DELETE(Blender)
	Blender = new PCGExDataBlending::FPropertiesBlender(BlendingSettings);
	return Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);
}

void UPCGExNeighborSampleProperties::PrepareNode(const PCGExCluster::FNode& TargetNode) const
{
	FPCGPoint& A = Cluster->VtxIO->GetMutablePoint(TargetNode.PointIndex);
	Blender->PrepareBlending(A, A);
}

void UPCGExNeighborSampleProperties::BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
{
	FPCGPoint& A = Cluster->VtxIO->GetMutablePoint(TargetNode.PointIndex);
	const FPCGPoint& B = Cluster->VtxIO->GetInPoint(Neighbor.Node->PointIndex);
	Blender->Blend(A, B, A, Weight);
}

void UPCGExNeighborSampleProperties::BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
{
	FPCGPoint& A = Cluster->VtxIO->GetMutablePoint(TargetNode.PointIndex);
	const FPCGPoint& B = Cluster->EdgesIO->GetInPoint(Neighbor.Edge->PointIndex);
	Blender->Blend(A, B, A, Weight);
}

void UPCGExNeighborSampleProperties::FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
{
	FPCGPoint& A = Cluster->VtxIO->GetMutablePoint(TargetNode.PointIndex);
	Blender->CompleteBlending(A, Count, TotalWeight);
}

void UPCGExNeighborSampleProperties::Cleanup()
{
	PCGEX_DELETE(Blender)
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSamplePropertiesSettings::GetDisplayName() const
{
	if (Descriptor.Blending.HasNoBlending()) { return TEXT("(None)"); }
	TArray<FName> Names;
	Descriptor.Blending.GetNonNoneBlendings(Names);

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
}
#endif

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryProperties::CreateOperation() const
{
	UPCGExNeighborSampleProperties* NewOperation = NewObject<UPCGExNeighborSampleProperties>();

	PCGEX_SAMPLER_CREATE

	NewOperation->BlendingSettings = Descriptor.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryProperties* SamplerFactory = NewObject<UPCGExNeighborSamplerFactoryProperties>();
	SamplerFactory->Descriptor = Descriptor;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

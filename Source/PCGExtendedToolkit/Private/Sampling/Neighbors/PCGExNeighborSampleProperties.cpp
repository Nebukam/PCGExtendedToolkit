// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleProperties.h"

#include "Data/Blending/PCGExPropertiesBlender.h"


#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void FPCGExNeighborSampleProperties::PrepareForCluster(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster> InCluster, const TSharedRef<PCGExData::FFacade> InVtxDataFacade, const TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	PropertiesBlender = MakeUnique<PCGExDataBlending::FPropertiesBlender>(BlendingDetails);
	return FPCGExNeighborSampleOperation::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);
}

void FPCGExNeighborSampleProperties::PrepareNode(const PCGExCluster::FNode& TargetNode) const
{
	FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
	PropertiesBlender->PrepareBlending(A, VtxDataFacade->Source->GetInPoint(TargetNode.PointIndex));
}

void FPCGExNeighborSampleProperties::SampleNeighborNode(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight)
{
	FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
	const FPCGPoint& B = VtxDataFacade->Source->GetInPoint(Cluster->GetNode(Lk)->PointIndex);
	PropertiesBlender->Blend(A, B, A, Weight);
}

void FPCGExNeighborSampleProperties::SampleNeighborEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight)
{
	FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
	const FPCGPoint& B = VtxDataFacade->Source->GetInPoint(Cluster->GetEdge(Lk)->PointIndex);
	PropertiesBlender->Blend(A, B, A, Weight);
}

void FPCGExNeighborSampleProperties::FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight)
{
	FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
	PropertiesBlender->CompleteBlending(A, Count, TotalWeight);
}

#if WITH_EDITOR
FString UPCGExNeighborSamplePropertiesSettings::GetDisplayName() const
{
	if (Config.Blending.HasNoBlending()) { return TEXT("(None)"); }
	TArray<FName> Names;
	Config.Blending.GetNonNoneBlendings(Names);

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
}
#endif

TSharedPtr<FPCGExNeighborSampleOperation> UPCGExNeighborSamplerFactoryProperties::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(NeighborSampleProperties)
	PCGEX_SAMPLER_CREATE_OPERATION

	NewOperation->BlendingDetails = Config.Blending;

	return NewOperation;
}

UPCGExFactoryData* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExNeighborSamplerFactoryProperties* SamplerFactory = InContext->ManagedObjects->New<UPCGExNeighborSamplerFactoryProperties>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

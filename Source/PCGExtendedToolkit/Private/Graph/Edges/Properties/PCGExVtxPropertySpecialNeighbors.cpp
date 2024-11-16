// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertySpecialNeighbors.h"


#define LOCTEXT_NAMESPACE "PCGExVtxPropertySpecialNeighbors"
#define PCGEX_NAMESPACE PCGExVtxPropertySpecialNeighbors

void UPCGExVtxPropertySpecialNeighbors::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExVtxPropertySpecialNeighbors* TypedOther = Cast<UPCGExVtxPropertySpecialNeighbors>(Other))
	{
		Config = TypedOther->Config;
	}
}

bool UPCGExVtxPropertySpecialNeighbors::PrepareForCluster(
	const FPCGContext* InContext,
	TSharedPtr<PCGExCluster::FCluster> InCluster,
	const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade,
	const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade)
{
	if (!Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade)) { return false; }

	if (!Config.LargestNeighbor.Validate(InContext) ||
		!Config.SmallestNeighbor.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Config.LargestNeighbor.Init(InVtxDataFacade.ToSharedRef());
	Config.SmallestNeighbor.Init(InVtxDataFacade.ToSharedRef());

	return bIsValidOperation;
}

void UPCGExVtxPropertySpecialNeighbors::ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	int32 LLargest = MIN_int32;
	int32 ILargest = -1;

	int32 LSmallest = MAX_int32;
	int32 ISmallest = -1;

	for (int i = 0; i < Adjacency.Num(); i++)
	{
		const PCGExCluster::FAdjacencyData& A = Adjacency[i];
		const int32 NumAdj = Cluster->GetNode(A.NodeIndex)->Num();

		if (NumAdj > LLargest)
		{
			ILargest = i;
			LLargest = NumAdj;
		}

		if (NumAdj < LSmallest)
		{
			ISmallest = i;
			LSmallest = NumAdj;
		}
	}

	if (ILargest != -1) { Config.LargestNeighbor.Set(Node.PointIndex, Adjacency[ILargest], Cluster->GetNode(Adjacency[ILargest].NodeIndex)->Num()); }
	else { Config.LargestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }

	if (ISmallest != -1) { Config.SmallestNeighbor.Set(Node.PointIndex, Adjacency[ISmallest], Cluster->GetNode(Adjacency[ISmallest].NodeIndex)->Num()); }
	else { Config.SmallestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

#if WITH_EDITOR
FString UPCGExVtxPropertySpecialNeighborsSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertySpecialNeighborsFactory::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExVtxPropertySpecialNeighbors* NewOperation = InContext->ManagedObjects->New<UPCGExVtxPropertySpecialNeighbors>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxPropertySpecialNeighborsSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertySpecialNeighborsFactory* NewFactory = InContext->ManagedObjects->New<UPCGExVtxPropertySpecialNeighborsFactory>();
	NewFactory->Config = Config;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

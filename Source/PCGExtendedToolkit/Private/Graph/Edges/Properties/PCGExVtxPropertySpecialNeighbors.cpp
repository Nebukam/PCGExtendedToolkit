// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertySpecialNeighbors.h"

#define LOCTEXT_NAMESPACE "PCGExVtxPropertySpecialNeighbors"
#define PCGEX_NAMESPACE PCGExVtxPropertySpecialNeighbors

#define PCGEX_FOREACH_FIELD_SPECIALEDGE(MACRO)\
MACRO(Shortest)\
MACRO(Longest)\
MACRO(Average)

void UPCGExVtxPropertySpecialNeighbors::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExVtxPropertySpecialNeighbors* TypedOther = Cast<UPCGExVtxPropertySpecialNeighbors>(Other))
	{
		Config = TypedOther->Config;
	}
}

bool UPCGExVtxPropertySpecialNeighbors::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade)
{
	if (!Super::PrepareForVtx(InContext, InVtxDataFacade)) { return false; }

	if (!Config.LargestNeighbor.Validate(InContext) ||
		!Config.SmallestNeighbor.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Config.LargestNeighbor.Init(InVtxDataFacade);
	Config.SmallestNeighbor.Init(InVtxDataFacade);

	return bIsValidOperation;
}

void UPCGExVtxPropertySpecialNeighbors::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	double LLargest = TNumericLimits<double>::Min();
	int32 ILargest = -1;

	double LSmallest = TNumericLimits<double>::Max();
	int32 ISmallest = -1;

	for (int i = 0; i < Adjacency.Num(); i++)
	{
		const PCGExCluster::FAdjacencyData& A = Adjacency[i];

		if (A.Length > LLargest)
		{
			ILargest = i;
			LLargest = A.Length;
		}

		if (A.Length < LSmallest)
		{
			ISmallest = i;
			LSmallest = A.Length;
		}
	}

	if (ILargest != -1) { Config.LargestNeighbor.Set(Node.PointIndex, Adjacency[ISmallest], (*Cluster->Nodes)[Adjacency[ISmallest].NodeIndex].Adjacency.Num()); }
	else { Config.LargestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }

	if (ISmallest != -1) { Config.SmallestNeighbor.Set(Node.PointIndex, Adjacency[ILargest], (*Cluster->Nodes)[Adjacency[ILargest].NodeIndex].Adjacency.Num()); }
	else { Config.SmallestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

#if WITH_EDITOR
FString UPCGExVtxPropertySpecialNeighborsSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertySpecialNeighborsFactory::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExVtxPropertySpecialNeighbors, NewOperation)
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxPropertySpecialNeighborsSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertySpecialNeighborsFactory* NewFactory = NewObject<UPCGExVtxPropertySpecialNeighborsFactory>();
	NewFactory->Config = Config;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef PCGEX_FOREACH_FIELD_SPECIALEDGE
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

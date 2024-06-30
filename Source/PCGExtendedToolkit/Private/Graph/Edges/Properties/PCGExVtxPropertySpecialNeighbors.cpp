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
	const UPCGExVtxPropertySpecialNeighbors* TypedOther = Cast<UPCGExVtxPropertySpecialNeighbors>(Other);
	if (TypedOther)
	{
		Descriptor = TypedOther->Descriptor;
	}
}

bool UPCGExVtxPropertySpecialNeighbors::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataCache)
{
	if (!Super::PrepareForVtx(InContext, InVtxDataCache)) { return false; }

	if (!Descriptor.LargestNeighbor.Validate(InContext) ||
		!Descriptor.SmallestNeighbor.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Descriptor.LargestNeighbor.Init(InVtxDataCache);
	Descriptor.SmallestNeighbor.Init(InVtxDataCache);

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

	if (ILargest != -1) { Descriptor.LargestNeighbor.Set(Node.PointIndex, Adjacency[ISmallest], (*Cluster->Nodes)[Adjacency[ISmallest].NodeIndex].Adjacency.Num()); }
	else { Descriptor.LargestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }

	if (ISmallest != -1) { Descriptor.SmallestNeighbor.Set(Node.PointIndex, Adjacency[ILargest], (*Cluster->Nodes)[Adjacency[ILargest].NodeIndex].Adjacency.Num()); }
	else { Descriptor.SmallestNeighbor.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

#if WITH_EDITOR
FString UPCGExVtxPropertySpecialNeighborsSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertySpecialNeighborsFactory::CreateOperation() const
{
	UPCGExVtxPropertySpecialNeighbors* NewOperation = NewObject<UPCGExVtxPropertySpecialNeighbors>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxPropertySpecialNeighborsSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertySpecialNeighborsFactory* NewFactory = NewObject<UPCGExVtxPropertySpecialNeighborsFactory>();
	NewFactory->Descriptor = Descriptor;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef PCGEX_FOREACH_FIELD_SPECIALEDGE
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

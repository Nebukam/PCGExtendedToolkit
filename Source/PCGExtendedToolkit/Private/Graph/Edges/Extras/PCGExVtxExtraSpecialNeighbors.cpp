// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Extras/PCGExVtxExtraSpecialNeighbors.h"

#define LOCTEXT_NAMESPACE "PCGExVtxExtraSpecialNeighbors"
#define PCGEX_NAMESPACE PCGExVtxExtraSpecialNeighbors

#define PCGEX_FOREACH_FIELD_SPECIALEDGE(MACRO)\
MACRO(Shortest)\
MACRO(Longest)\
MACRO(Average)

void UPCGExVtxExtraSpecialNeighbors::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxExtraSpecialNeighbors* TypedOther = Cast<UPCGExVtxExtraSpecialNeighbors>(Other);
	if (TypedOther)
	{
		Descriptor = TypedOther->Descriptor;
	}
}

bool UPCGExVtxExtraSpecialNeighbors::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataCache)
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

void UPCGExVtxExtraSpecialNeighbors::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
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

void UPCGExVtxExtraSpecialNeighbors::Cleanup()
{
	Super::Cleanup();
	Descriptor.LargestNeighbor.Cleanup();
	Descriptor.SmallestNeighbor.Cleanup();
}

#if WITH_EDITOR
FString UPCGExVtxExtraSpecialNeighborsSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxExtraOperation* UPCGExVtxExtraSpecialNeighborsFactory::CreateOperation() const
{
	UPCGExVtxExtraSpecialNeighbors* NewOperation = NewObject<UPCGExVtxExtraSpecialNeighbors>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxExtraSpecialNeighborsSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxExtraSpecialNeighborsFactory* NewFactory = NewObject<UPCGExVtxExtraSpecialNeighborsFactory>();
	NewFactory->Descriptor = Descriptor;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef PCGEX_FOREACH_FIELD_SPECIALEDGE
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

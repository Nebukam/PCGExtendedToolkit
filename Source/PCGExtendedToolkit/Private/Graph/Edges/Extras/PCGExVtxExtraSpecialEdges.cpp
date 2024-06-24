// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Extras/PCGExVtxExtraSpecialEdges.h"

#define LOCTEXT_NAMESPACE "PCGExVtxExtraSpecialEdges"
#define PCGEX_NAMESPACE PCGExVtxExtraSpecialEdges

#define PCGEX_FOREACH_FIELD_SPECIALEDGE(MACRO)\
MACRO(Shortest)\
MACRO(Longest)\
MACRO(Average)

void UPCGExVtxExtraSpecialEdges::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxExtraSpecialEdges* TypedOther = Cast<UPCGExVtxExtraSpecialEdges>(Other);
	if (TypedOther)
	{
		Descriptor = TypedOther->Descriptor;
	}
}

bool UPCGExVtxExtraSpecialEdges::PrepareForVtx(const FPCGContext* InContext, PCGExData::FPointIO* InVtx)
{
	if (!Super::PrepareForVtx(InContext, InVtx)) { return false; }

	if (!Descriptor.ShortestEdge.Validate(InContext) ||
		!Descriptor.LongestEdge.Validate(InContext) ||
		!Descriptor.AverageEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Descriptor.ShortestEdge.Init(InVtx);
	Descriptor.LongestEdge.Init(InVtx);
	Descriptor.AverageEdge.Init(InVtx);

	return bIsValidOperation;
}

void UPCGExVtxExtraSpecialEdges::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	double LLongest = TNumericLimits<double>::Min();
	int32 ILongest = -1;

	double LShortest = TNumericLimits<double>::Max();
	int32 IShortest = -1;

	double LAverage = 0;
	FVector VAverage = FVector::Zero();

	for (int i = 0; i < Adjacency.Num(); i++)
	{
		const PCGExCluster::FAdjacencyData& A = Adjacency[i];

		if (A.Length > LLongest)
		{
			ILongest = i;
			LLongest = A.Length;
		}

		if (A.Length < LShortest)
		{
			IShortest = i;
			LShortest = A.Length;
		}

		LAverage += A.Length;
		VAverage += A.Direction;
	}

	LAverage /= Adjacency.Num();
	VAverage /= Adjacency.Num();

	Descriptor.AverageEdge.Set(Node.PointIndex, LAverage, VAverage);

	if (ILongest != -1) { Descriptor.LongestEdge.Set(Node.PointIndex, Adjacency[IShortest], (*Cluster->Nodes)[Adjacency[IShortest].NodeIndex].Adjacency.Num()); }
	else { Descriptor.LongestEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }

	if (IShortest != -1) { Descriptor.ShortestEdge.Set(Node.PointIndex, Adjacency[ILongest], (*Cluster->Nodes)[Adjacency[ILongest].NodeIndex].Adjacency.Num()); }
	else { Descriptor.ShortestEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

void UPCGExVtxExtraSpecialEdges::Write()
{
	Super::Write();
	Descriptor.ShortestEdge.Write();
	Descriptor.LongestEdge.Write();
	Descriptor.AverageEdge.Write();
}

void UPCGExVtxExtraSpecialEdges::Write(PCGExMT::FTaskManager* AsyncManager)
{
	Super::Write(AsyncManager);
	Descriptor.ShortestEdge.Write(AsyncManager);
	Descriptor.LongestEdge.Write(AsyncManager);
	Descriptor.AverageEdge.Write(AsyncManager);
}

void UPCGExVtxExtraSpecialEdges::Cleanup()
{
	Super::Cleanup();
	Descriptor.ShortestEdge.Cleanup();
	Descriptor.LongestEdge.Cleanup();
	Descriptor.AverageEdge.Cleanup();
}

#if WITH_EDITOR
FString UPCGExVtxExtraSpecialEdgesSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxExtraOperation* UPCGExVtxExtraSpecialEdgesFactory::CreateOperation() const
{
	UPCGExVtxExtraSpecialEdges* NewOperation = NewObject<UPCGExVtxExtraSpecialEdges>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxExtraSpecialEdgesSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxExtraSpecialEdgesFactory* NewFactory = NewObject<UPCGExVtxExtraSpecialEdgesFactory>();
	NewFactory->Descriptor = Descriptor;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef PCGEX_FOREACH_FIELD_SPECIALEDGE
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

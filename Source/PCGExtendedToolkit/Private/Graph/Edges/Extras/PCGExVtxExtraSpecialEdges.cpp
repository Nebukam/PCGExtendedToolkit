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

bool UPCGExVtxExtraSpecialEdges::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster)
{
	if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

	if (!Descriptor.ShortestEdge.Validate(InContext) ||
		!Descriptor.LongestEdge.Validate(InContext) ||
		!Descriptor.AverageEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Descriptor.ShortestEdge.Init(Cluster);
	Descriptor.LongestEdge.Init(Cluster);
	Descriptor.LongestEdge.Init(Cluster);

	return bIsValidOperation;
}

void UPCGExVtxExtraSpecialEdges::ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
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

	Descriptor.AverageEdge.Set(Node.PointIndex, LAverage, VAverage, -1, -1);
	if (ILongest != -1) { Descriptor.LongestEdge.Set(Node.PointIndex, Adjacency[IShortest]); }
	if (IShortest != -1) { Descriptor.ShortestEdge.Set(Node.PointIndex, Adjacency[ILongest]); }
}

void UPCGExVtxExtraSpecialEdges::Write()
{
	Super::Write();
	Descriptor.ShortestEdge.Write();
	Descriptor.LongestEdge.Write();
	Descriptor.AverageEdge.Write();
}

void UPCGExVtxExtraSpecialEdges::Write(const TArrayView<int32> Indices)
{
	Super::Write(Indices);
	Descriptor.ShortestEdge.Write(Indices);
	Descriptor.LongestEdge.Write(Indices);
	Descriptor.AverageEdge.Write(Indices);
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

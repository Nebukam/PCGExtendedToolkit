// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertySpecialEdges.h"


#define LOCTEXT_NAMESPACE "PCGExVtxPropertySpecialEdges"
#define PCGEX_NAMESPACE PCGExVtxPropertySpecialEdges

#define PCGEX_FOREACH_FIELD_SPECIALEDGE(MACRO)\
MACRO(Shortest)\
MACRO(Longest)\
MACRO(Average)

void UPCGExVtxPropertySpecialEdges::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxPropertySpecialEdges* TypedOther = Cast<UPCGExVtxPropertySpecialEdges>(Other);
	if (TypedOther)
	{
		Config = TypedOther->Config;
	}
}

bool UPCGExVtxPropertySpecialEdges::PrepareForVtx(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade)
{
	if (!Super::PrepareForVtx(InContext, InVtxDataFacade)) { return false; }

	if (!Config.ShortestEdge.Validate(InContext) ||
		!Config.LongestEdge.Validate(InContext) ||
		!Config.AverageEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	Config.ShortestEdge.Init(InVtxDataFacade.ToSharedRef());
	Config.LongestEdge.Init(InVtxDataFacade.ToSharedRef());
	Config.AverageEdge.Init(InVtxDataFacade.ToSharedRef());

	return bIsValidOperation;
}

void UPCGExVtxPropertySpecialEdges::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	double LLongest = MIN_dbl;
	int32 ILongest = -1;

	double LShortest = MAX_dbl;
	int32 IShortest = -1;

	double LAverage = 0;
	FVector VAverage = FVector::Zero();

	for (int i = 0; i < Adjacency.Num(); ++i)
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

	Config.AverageEdge.Set(Node.PointIndex, LAverage, VAverage);

	if (ILongest != -1) { Config.LongestEdge.Set(Node.PointIndex, Adjacency[IShortest], (*Cluster->Nodes)[Adjacency[IShortest].NodeIndex].Adjacency.Num()); }
	else { Config.LongestEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }

	if (IShortest != -1) { Config.ShortestEdge.Set(Node.PointIndex, Adjacency[ILongest], (*Cluster->Nodes)[Adjacency[ILongest].NodeIndex].Adjacency.Num()); }
	else { Config.ShortestEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

#if WITH_EDITOR
FString UPCGExVtxPropertySpecialEdgesSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertySpecialEdgesFactory::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExVtxPropertySpecialEdges* NewOperation = InContext->NewManagedObject<UPCGExVtxPropertySpecialEdges>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxPropertySpecialEdgesSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertySpecialEdgesFactory* NewFactory = NewObject<UPCGExVtxPropertySpecialEdgesFactory>();
	NewFactory->Config = Config;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef PCGEX_FOREACH_FIELD_SPECIALEDGE
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertyOrient.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExVtxPropertyOrient"
#define PCGEX_NAMESPACE PCGExVtxPropertyOrient

void UPCGExVtxPropertyOrient::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxPropertyOrient* TypedOther = Cast<UPCGExVtxPropertyOrient>(Other);
	if (TypedOther)
	{
		Descriptor = TypedOther->Descriptor;
	}
}

void UPCGExVtxPropertyOrient::PrepareForCluster(const FPCGContext* InContext, const int32 ClusterIdx, PCGExCluster::FCluster* Cluster, PCGExData::FFacade* VtxDataFacade, PCGExData::FFacade* EdgeDataFacade)
{
	Super::PrepareForCluster(InContext, ClusterIdx, Cluster, VtxDataFacade, EdgeDataFacade);
}

bool UPCGExVtxPropertyOrient::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade)
{
	if (!Super::PrepareForVtx(InContext, InVtxDataFacade)) { return false; }

	return bIsValidOperation;
}

void UPCGExVtxPropertyOrient::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	const FPCGPoint& Point = PrimaryDataFacade->Source->GetInPoint(Node.PointIndex);
}

void UPCGExVtxPropertyOrient::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExVtxPropertyOrientSettings::GetDisplayName() const
{
	/*
	if (Descriptor.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = Descriptor.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
	*/
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertyOrientFactory::CreateOperation() const
{
	UPCGExVtxPropertyOrient* NewOperation = NewObject<UPCGExVtxPropertyOrient>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExVtxPropertyOrientSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertyOrientFactory* NewFactory = NewObject<UPCGExVtxPropertyOrientFactory>();
	NewFactory->Descriptor = Descriptor;
	GetInputFactories(
		InContext, PCGEx::SourceAdditionalReq, NewFactory->FilterFactories,
		PCGExFactories::ClusterEdgeFilters, false);
	return Super::CreateFactory(InContext, NewFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

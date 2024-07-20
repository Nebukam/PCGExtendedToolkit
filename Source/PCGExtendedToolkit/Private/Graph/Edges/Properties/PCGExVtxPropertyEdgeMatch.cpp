// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Properties/PCGExVtxPropertyEdgeMatch.h"

#include "PCGPin.h"
#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExVtxPropertyEdgeMatch"
#define PCGEX_NAMESPACE PCGExVtxPropertyEdgeMatch

void UPCGExVtxPropertyEdgeMatch::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxPropertyEdgeMatch* TypedOther = Cast<UPCGExVtxPropertyEdgeMatch>(Other);
	if (TypedOther)
	{
		Config = TypedOther->Config;
	}
}

void UPCGExVtxPropertyEdgeMatch::ClusterReserve(const int32 NumClusters)
{
	Super::ClusterReserve(NumClusters);
	FilterManagers.SetNumUninitialized(NumClusters);
	for (int i = 0; i < NumClusters; i++) { FilterManagers[i] = nullptr; }
}

void UPCGExVtxPropertyEdgeMatch::PrepareForCluster(const FPCGContext* InContext, const int32 ClusterIdx, PCGExCluster::FCluster* Cluster, PCGExData::FFacade* VtxDataFacade, PCGExData::FFacade* EdgeDataFacade)
{
	Super::PrepareForCluster(InContext, ClusterIdx, Cluster, VtxDataFacade, EdgeDataFacade);
	if (FilterFactories && !FilterFactories->IsEmpty())
	{
		// TODO
	}
}

bool UPCGExVtxPropertyEdgeMatch::PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade)
{
	if (!Super::PrepareForVtx(InContext, InVtxDataFacade)) { return false; }

	if (!Config.MatchingEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	if (!Config.DotComparisonDetails.Init(InContext, InVtxDataFacade))
	{
		bIsValidOperation = false;
		return false;
	}

	if (Config.DirectionSource == EPCGExFetchType::Attribute)
	{
		DirCache = PrimaryDataFacade->GetBroadcaster<FVector>(Config.Direction);
		if (!DirCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Direction attribute is invalid"));
			bIsValidOperation = false;
			return false;
		}
	}

	Config.MatchingEdge.Init(InVtxDataFacade);

	return bIsValidOperation;
}

void UPCGExVtxPropertyEdgeMatch::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	//PCGExPointFilter::TManager* EdgeFilters = FilterManagers[ClusterIdx]; //TODO : Implement properly

	const FPCGPoint& Point = PrimaryDataFacade->Source->GetInPoint(Node.PointIndex);

	double BestDot = TNumericLimits<double>::Min();
	int32 IBest = -1;
	const double DotB = Config.DotComparisonDetails.GetDot(Node.PointIndex);

	FVector NodeDirection = DirCache ? DirCache->Values[Node.PointIndex].GetSafeNormal() : Config.DirectionConstant;
	if (Config.bTransformDirection) { NodeDirection = Point.Transform.TransformVectorNoScale(NodeDirection); }

	for (int i = 0; i < Adjacency.Num(); i++)
	{
		const PCGExCluster::FAdjacencyData& A = Adjacency[i];
		const double DotA = FVector::DotProduct(NodeDirection, A.Direction);

		if (Config.DotComparisonDetails.Test(DotA, DotB))
		{
			if (DotA > BestDot)
			{
				BestDot = DotA;
				IBest = i;
			}
		}
	}

	if (IBest != -1) { Config.MatchingEdge.Set(Node.PointIndex, Adjacency[IBest], (*Cluster->Nodes)[Adjacency[IBest].NodeIndex].Adjacency.Num()); }
	else { Config.MatchingEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

void UPCGExVtxPropertyEdgeMatch::Cleanup()
{
	PCGEX_DELETE_TARRAY(FilterManagers)
	Super::Cleanup();
}

void UPCGExVtxPropertyEdgeMatch::InitEdgeFilters()
{
	if (bEdgeFilterInitialized) { return; }

	FWriteScopeLock WriteScopeLock(FilterLock);

	bEdgeFilterInitialized = true;
}

#if WITH_EDITOR
FString UPCGExVtxPropertyEdgeMatchSettings::GetDisplayName() const
{
	/*
	if (Config.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = Config.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
	*/
	return TEXT("");
}
#endif

UPCGExVtxPropertyOperation* UPCGExVtxPropertyEdgeMatchFactory::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExVtxPropertyEdgeMatch, NewOperation)
	PCGEX_VTX_EXTRA_CREATE

	if (!FilterFactories.IsEmpty())
	{
		NewOperation->FilterFactories = const_cast<TArray<UPCGExFilterFactoryBase*>*>(&FilterFactories);
	}

	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxPropertyEdgeMatchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourceAdditionalReq, "Additional Requirements for the match", Advanced, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExVtxPropertyEdgeMatchSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertyEdgeMatchFactory* NewFactory = NewObject<UPCGExVtxPropertyEdgeMatchFactory>();
	NewFactory->Config = Config;
	NewFactory->Config.Sanitize();
	GetInputFactories(
		InContext, PCGEx::SourceAdditionalReq, NewFactory->FilterFactories,
		PCGExFactories::ClusterEdgeFilters, false);
	return Super::CreateFactory(InContext, NewFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

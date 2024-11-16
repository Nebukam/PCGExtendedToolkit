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
	if (const UPCGExVtxPropertyEdgeMatch* TypedOther = Cast<UPCGExVtxPropertyEdgeMatch>(Other))
	{
		Config = TypedOther->Config;
	}
}

bool UPCGExVtxPropertyEdgeMatch::PrepareForCluster(const FPCGContext* InContext, TSharedPtr<PCGExCluster::FCluster> InCluster, const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade)
{
	if (!Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade)) { return false; }

	if (!Config.MatchingEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	if (!Config.DotComparisonDetails.Init(InContext, InVtxDataFacade.ToSharedRef()))
	{
		bIsValidOperation = false;
		return false;
	}

	if (Config.DirectionInput == EPCGExInputValueType::Attribute)
	{
		DirCache = PrimaryDataFacade->GetBroadcaster<FVector>(Config.Direction);
		if (!DirCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Direction attribute is invalid"));
			bIsValidOperation = false;
			return false;
		}
	}

	Config.MatchingEdge.Init(InVtxDataFacade.ToSharedRef());

	return bIsValidOperation;
}

void UPCGExVtxPropertyEdgeMatch::ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	const FPCGPoint& Point = PrimaryDataFacade->Source->GetInPoint(Node.PointIndex);

	double BestDot = MIN_dbl;
	int32 IBest = -1;
	const double DotB = Config.DotComparisonDetails.GetDot(Node.PointIndex);

	FVector NodeDirection = DirCache ? DirCache->Read(Node.PointIndex).GetSafeNormal() : Config.DirectionConstant;
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

	if (IBest != -1) { Config.MatchingEdge.Set(Node.PointIndex, Adjacency[IBest], Cluster->GetNode(Adjacency[IBest].NodeIndex)->Num()); }
	else { Config.MatchingEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
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

UPCGExVtxPropertyOperation* UPCGExVtxPropertyEdgeMatchFactory::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExVtxPropertyEdgeMatch* NewOperation = InContext->ManagedObjects->New<UPCGExVtxPropertyEdgeMatch>();
	PCGEX_VTX_EXTRA_CREATE
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxPropertyEdgeMatchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExVtxPropertyEdgeMatchSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxPropertyEdgeMatchFactory* NewFactory = InContext->ManagedObjects->New<UPCGExVtxPropertyEdgeMatchFactory>();
	NewFactory->Config = Config;
	NewFactory->Config.Sanitize();
	return Super::CreateFactory(InContext, NewFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

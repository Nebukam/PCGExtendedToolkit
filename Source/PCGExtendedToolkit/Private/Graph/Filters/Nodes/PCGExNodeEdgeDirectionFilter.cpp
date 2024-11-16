// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExNodeEdgeDirectionFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeEdgeDirectionFilter"
#define PCGEX_NAMESPACE NodeEdgeDirectionFilter

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNodeEdgeDirectionFilterFactory::CreateFilter() const
{
	return MakeShared<FNodeEdgeDirectionFilter>(this);
}

bool FNodeEdgeDirectionFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
{
	if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

	DirConstant = TypedFilterFactory->Config.DirectionConstant.GetSafeNormal();
	bFromNode = TypedFilterFactory->Config.DirectionOrder == EPCGExAdjacencyDirectionOrigin::FromNode;

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		OperandDirection = PointDataFacade->GetBroadcaster<FVector>(TypedFilterFactory->Config.Direction);
		if (!OperandDirection)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Direction attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.Direction.GetName())));
			return false;
		}
	}

	if (!Adjacency.Init(InContext, PointDataFacade.ToSharedRef())) { return false; }

	if (TypedFilterFactory->Config.ComparisonQuality == EPCGExDirectionCheckMode::Dot)
	{
		if (!DotComparison.Init(InContext, PointDataFacade.ToSharedRef())) { return false; }
	}
	else
	{
		bUseDot = false;
		if (!HashComparison.Init(InContext, PointDataFacade.ToSharedRef())) { return false; }
	}

	return true;
}

bool FNodeEdgeDirectionFilter::Test(const PCGExCluster::FNode& Node) const
{
	return bUseDot ? TestDot(Node) : TestHash(Node);
}

bool FNodeEdgeDirectionFilter::TestDot(const PCGExCluster::FNode& Node) const
{
	const int32 PointIndex = Node.PointIndex;
	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

	const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);

	FVector RefDir = OperandDirection ? OperandDirection->Read(PointIndex) : DirConstant;
	if (TypedFilterFactory->Config.bTransformDirection) { RefDir = Point.Transform.TransformVectorNoScale(RefDir).GetSafeNormal(); }

	const double A = DotComparison.GetDot(PointIndex);
	double B = 0;

	TArray<double> Dots;
	Dots.SetNumUninitialized(Node.Num());

	// Precompute all dot products

	if (bFromNode)
	{
		if (DotComparison.bUnsignedDot)
		{
			for (int i = 0; i < Dots.Num(); i++)
			{
				Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node)));
			}
		}
		else
		{
			for (int i = 0; i < Dots.Num(); i++)
			{
				Dots[i] = FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node));
			}
		}
	}
	else
	{
		if (DotComparison.bUnsignedDot)
		{
			for (int i = 0; i < Dots.Num(); i++)
			{
				Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node)));
			}
		}
		else
		{
			for (int i = 0; i < Dots.Num(); i++)
			{
				Dots[i] = FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node));
			}
		}
	}

	if (Adjacency.bTestAllNeighbors)
	{
		if (Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			for (const double Dot : Dots) { if (!DotComparison.Test(A, Dot)) { return false; } }
			return true;
		}

		// First, build the consolidated operand B
		switch (Adjacency.Consolidation)
		{
		default:
		case EPCGExAdjacencyGatherMode::Average:
			for (const double Dot : Dots) { B += Dot; }
			B /= Node.Links.Num();
			break;
		case EPCGExAdjacencyGatherMode::Min:
			B = MAX_dbl;
			for (const double Dot : Dots) { B = FMath::Min(B, Dot); }
			break;
		case EPCGExAdjacencyGatherMode::Max:
			B = MIN_dbl;
			for (const double Dot : Dots) { B = FMath::Max(B, Dot); }
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			for (const double Dot : Dots) { B += Dot; }
			break;
		}

		return DotComparison.Test(A, B);
	}

	// Only some adjacent samples must pass the comparison
	const int32 Threshold = Adjacency.GetThreshold(Node);

	// Early exit on impossible thresholds (i.e node has less neighbor that the minimum or exact requirements)		
	if (Threshold == -1) { return false; }

	int32 LocalSuccessCount = 0;
	for (const double Dot : Dots) { if (DotComparison.Test(A, Dot)) { LocalSuccessCount++; } }

	return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold);
}

bool FNodeEdgeDirectionFilter::TestHash(const PCGExCluster::FNode& Node) const
{
	const int32 PointIndex = Node.PointIndex;
	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

	const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);

	FVector RefDir = OperandDirection ? OperandDirection->Read(PointIndex) : DirConstant;
	if (TypedFilterFactory->Config.bTransformDirection) { RefDir = Point.Transform.TransformVectorNoScale(RefDir); }

	RefDir.Normalize();

	const FVector CWTolerance = HashComparison.GetCWTolerance(PointIndex);
	const FInt32Vector A = PCGEx::I323(RefDir, CWTolerance);

	TArray<FInt32Vector> Hashes;
	Hashes.SetNumUninitialized(Node.Links.Num());

	// Precompute all dot products

	if (bFromNode)
	{
		for (int i = 0; i < Hashes.Num(); i++)
		{
			Hashes[i] = PCGEx::I323(Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node), CWTolerance);
		}
	}
	else
	{
		for (int i = 0; i < Hashes.Num(); i++)
		{
			Hashes[i] = PCGEx::I323(Cluster->GetDir(Node.NodeIndex, Node.Links[i].Node), CWTolerance);
		}
	}

	if (Adjacency.bTestAllNeighbors)
	{
		for (const FInt32Vector Hash : Hashes) { if (A != Hash) { return false; } }
		return true;
	}

	// Only some adjacent samples must pass the comparison
	const int32 Threshold = Adjacency.GetThreshold(Node);

	// Early exit on impossible thresholds (i.e node has less neighbor that the minimum or exact requirements)		
	if (Threshold == -1) { return false; }

	int32 LocalSuccessCount = 0;
	for (const FInt32Vector Hash : Hashes) { if (A == Hash) { LocalSuccessCount++; } }

	return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold);
}


PCGEX_CREATE_FILTER_FACTORY(NodeEdgeDirection)

#if WITH_EDITOR
FString UPCGExNodeEdgeDirectionFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Edge Direction ") + PCGExCompare::ToString(Config.DotComparisonDetails.Comparison);

	UPCGExNodeEdgeDirectionFilterProviderSettings* MutableSelf = const_cast<UPCGExNodeEdgeDirectionFilterProviderSettings*>(this);

	DisplayName += PCGEx::GetSelectorDisplayName(Config.Direction);
	DisplayName += TEXT(" (");

	switch (Config.Adjacency.Mode)
	{
	case EPCGExAdjacencyTestMode::All:
		DisplayName += TEXT("All");
		break;
	case EPCGExAdjacencyTestMode::Some:
		DisplayName += TEXT("Some");
		break;
	default: ;
	}

	DisplayName += TEXT(")");
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

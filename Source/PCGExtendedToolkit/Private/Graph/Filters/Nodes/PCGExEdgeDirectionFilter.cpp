// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExEdgeDirectionFilter.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeEdgeDirectionFilter"
#define PCGEX_NAMESPACE NodeEdgeDirectionFilter

PCGExPointFilter::TFilter* UPCGExEdgeDirectionFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::FEdgeDirectionFilter(this);
}

namespace PCGExNodeAdjacency
{
	bool FEdgeDirectionFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade)
	{
		if (!TFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		bFromNode = TypedFilterFactory->Config.DirectionOrder == EPCGExAdjacencyDirectionOrigin::FromNode;

		if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandDirection = PointDataFacade->GetBroadcaster<FVector>(TypedFilterFactory->Config.Direction);
			if (!OperandDirection)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Direction attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.Direction.GetName())));
				return false;
			}
		}

		if (!Adjacency.Init(InContext, PointDataFacade)) { return false; }

		if (TypedFilterFactory->Config.ComparisonQuality == EPCGExDirectionCheckMode::Dot)
		{
			if (!DotComparison.Init(InContext, PointDataFacade)) { return false; }
		}
		else
		{
			bUseDot = false;
			if (!HashComparison.Init(InContext, PointDataFacade)) { return false; }
		}

		return true;
	}

	bool FEdgeDirectionFilter::Test(const PCGExCluster::FNode& Node) const
	{
		return bUseDot ? TestDot(Node) : TestHash(Node);
	}

	bool FEdgeDirectionFilter::TestDot(const PCGExCluster::FNode& Node) const
	{
		const int32 PointIndex = Node.PointIndex;
		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

		const FPCGPoint& Point = Cluster->VtxIO->GetInPoint(PointIndex);

		FVector RefDir = OperandDirection ? OperandDirection->Values[PointIndex] : TypedFilterFactory->Config.DirectionConstant;
		if (TypedFilterFactory->Config.bTransformDirection) { RefDir = Point.Transform.TransformVectorNoScale(RefDir).GetSafeNormal(); }

		const double A = DotComparison.GetDot(PointIndex);
		double B = 0;

		TArray<double> Dots;
		Dots.SetNumUninitialized(Node.Adjacency.Num());

		// Precompute all dot products

		if (bFromNode)
		{
			if (DotComparison.bUnsignedDot)
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i]))));
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					Dots[i] = FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i])));
				}
			}
		}
		else
		{
			if (DotComparison.bUnsignedDot)
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i]))));
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					Dots[i] = FVector::DotProduct(RefDir, Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i])));
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
				B /= Node.Adjacency.Num();
				break;
			case EPCGExAdjacencyGatherMode::Min:
				B = TNumericLimits<double>::Max();
				for (const double Dot : Dots) { B = FMath::Min(B, Dot); }
				break;
			case EPCGExAdjacencyGatherMode::Max:
				B = TNumericLimits<double>::Min();
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

	bool FEdgeDirectionFilter::TestHash(const PCGExCluster::FNode& Node) const
	{
		const int32 PointIndex = Node.PointIndex;
		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

		const FPCGPoint& Point = Cluster->VtxIO->GetInPoint(PointIndex);

		FVector RefDir = OperandDirection ? OperandDirection->Values[PointIndex] : TypedFilterFactory->Config.DirectionConstant;
		if (TypedFilterFactory->Config.bTransformDirection) { RefDir = Point.Transform.TransformVectorNoScale(RefDir).GetSafeNormal(); }

		const FVector CWTolerance = HashComparison.GetCWTolerance(PointIndex);
		const uint32 A = PCGEx::GH(RefDir, CWTolerance);

		TArray<uint32> Hashes;
		Hashes.SetNumUninitialized(Node.Adjacency.Num());

		// Precompute all dot products

		if (bFromNode)
		{
			for (int i = 0; i < Hashes.Num(); i++)
			{
				Hashes[i] = PCGEx::GH(Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i])), CWTolerance);
			}
		}
		else
		{
			for (int i = 0; i < Hashes.Num(); i++)
			{
				Hashes[i] = PCGEx::GH(Cluster->GetDir(Node.NodeIndex, PCGEx::H64A(Node.Adjacency[i])), CWTolerance);
			}
		}

		if (Adjacency.bTestAllNeighbors)
		{
			for (const double Hash : Hashes) { if (A != Hash) { return false; } }
			return true;
		}

		// Only some adjacent samples must pass the comparison
		const int32 Threshold = Adjacency.GetThreshold(Node);

		// Early exit on impossible thresholds (i.e node has less neighbor that the minimum or exact requirements)		
		if (Threshold == -1) { return false; }

		int32 LocalSuccessCount = 0;
		for (const double Hash : Hashes) { if (A == Hash) { LocalSuccessCount++; } }

		return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold);
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeDirection)

#if WITH_EDITOR
FString UPCGExEdgeDirectionFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Edge Direction ") + PCGExCompare::ToString(Config.DotComparisonDetails.Comparison);

	UPCGExEdgeDirectionFilterProviderSettings* MutableSelf = const_cast<UPCGExEdgeDirectionFilterProviderSettings*>(this);
	MutableSelf->Config.DirectionConstant = Config.DirectionConstant.GetSafeNormal();

	DisplayName += Config.Direction.GetName().ToString();
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

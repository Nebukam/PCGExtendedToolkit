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
	bool FEdgeDirectionFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataCache, PCGExData::FFacade* InEdgeDataCache)
	{
		if (!TFilter::Init(InContext, InCluster, InPointDataCache, InEdgeDataCache)) { return false; }

		bFromNode = TypedFilterFactory->Descriptor.Origin == EPCGExAdjacencyDirectionOrigin::FromNode;

		if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandDirection = PointDataCache->GetOrCreateGetter<FVector>(TypedFilterFactory->Descriptor.Direction);
			if (!OperandDirection)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Direction attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Direction.GetName())));
				return false;
			}
		}

		if (!Adjacency.Init(InContext, PointDataCache)) { return false; }

		if (TypedFilterFactory->Descriptor.ComparisonQuality == EPCGExDirectionCheckMode::Dot)
		{
			if (!DotComparison.Init(InContext, PointDataCache)) { return false; }
		}
		else
		{
			bUseDot = false;
			if (!HashComparison.Init(InContext, PointDataCache)) { return false; }
		}

		return true;
	}

	bool FEdgeDirectionFilter::Test(const PCGExCluster::FNode& Node) const
	{
		return bUseDot ? TestDot(Node.PointIndex) : TestHash(Node.NodeIndex);
	}

	bool FEdgeDirectionFilter::TestDot(const int32 PointIndex) const
	{
		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

		const PCGExCluster::FNode& Node = NodesRef[PointIndex];
		const FPCGPoint& Point = Cluster->VtxIO->GetInPoint(Node.PointIndex);

		const FVector RefDir = TypedFilterFactory->Descriptor.bTransformDirection ?
			                       Point.Transform.TransformVectorNoScale(OperandDirection->Values[Node.PointIndex].GetSafeNormal()) :
			                       OperandDirection->Values[Node.PointIndex].GetSafeNormal();
		const double A = DotComparison.GetDot(Node.PointIndex);
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
					FVector Direction = (NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position - Node.Position).GetSafeNormal();
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Direction));
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position - Node.Position).GetSafeNormal();
					Dots[i] = FVector::DotProduct(RefDir, Direction);
				}
			}
		}
		else
		{
			if (DotComparison.bUnsignedDot)
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (Node.Position - NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position).GetSafeNormal();
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Direction));
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (Node.Position - NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position).GetSafeNormal();
					Dots[i] = FVector::DotProduct(RefDir, Direction);
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

	bool FEdgeDirectionFilter::TestHash(const int32 PointIndex) const
	{
		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

		const PCGExCluster::FNode& Node = NodesRef[PointIndex];
		const FPCGPoint& Point = Cluster->VtxIO->GetInPoint(Node.PointIndex);

		const FVector RefDir = TypedFilterFactory->Descriptor.bTransformDirection ?
			                       Point.Transform.TransformVectorNoScale(OperandDirection->Values[Node.PointIndex].GetSafeNormal()) :
			                       OperandDirection->Values[Node.PointIndex].GetSafeNormal();

		const FVector CWTolerance = HashComparison.GetCWTolerance(Node.PointIndex);
		const uint64 A = PCGEx::GH(RefDir, CWTolerance);

		TArray<uint64> Hashes;
		Hashes.SetNumUninitialized(Node.Adjacency.Num());

		// Precompute all dot products

		if (bFromNode)
		{
			for (int i = 0; i < Hashes.Num(); i++)
			{
				FVector Direction = (NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position - Node.Position).GetSafeNormal();
				Hashes[i] = PCGEx::GH(Direction, CWTolerance);
			}
		}
		else
		{
			for (int i = 0; i < Hashes.Num(); i++)
			{
				FVector Direction = (Node.Position - NodesRef[PCGEx::H64A(Node.Adjacency[i])].Position).GetSafeNormal();
				Hashes[i] = PCGEx::GH(Direction, CWTolerance);
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
	FString DisplayName = TEXT("Edge Direction ") + PCGExCompare::ToString(Descriptor.Comparison);

	DisplayName += Descriptor.Direction.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Descriptor.Adjacency.Mode)
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

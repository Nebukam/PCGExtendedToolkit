// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExEdgeDirectionFilter.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeEdgeDirectionFilter"
#define PCGEX_NAMESPACE NodeEdgeDirectionFilter

PCGExDataFilter::TFilter* UPCGExEdgeDirectionFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::TEdgeDirectionFilter(this);
}

namespace PCGExNodeAdjacency
{
	PCGExDataFilter::EType TEdgeDirectionFilter::GetFilterType() const { return PCGExDataFilter::EType::ClusterNode; }

	void TEdgeDirectionFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bFromNode = TypedFilterFactory->Descriptor.Origin == EPCGExAdjacencyDirectionOrigin::FromNode;

		TFilter::Capture(InContext, PointIO);

		auto ExitFail = [&]()
		{
			bValid = false;

			Adjacency.Cleanup();
			DotComparison.Cleanup();
			PCGEX_DELETE(OperandDirection)
		};

		if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandDirection = new PCGEx::FLocalVectorGetter();
			OperandDirection->Capture(TypedFilterFactory->Descriptor.Direction);

			if (!OperandDirection->Grab(PointIO, false))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Direction attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Direction.GetName())));
				return ExitFail();
			}
		}

		if (!Adjacency.Init(InContext, PointIO)) { return ExitFail(); }
		if (!DotComparison.Init(InContext, PointIO)) { return ExitFail(); }
	}

	bool TEdgeDirectionFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TClusterNodeFilter::PrepareForTesting(PointIO);

		if (!Adjacency.bTestAllNeighbors)
		{
			// TODO : Test whether or not it's more efficient to cache the thresholds first or not (would need to grab Adjacency first instead of soft grab)
			/*
			const int32 NumNodes = CapturedCluster->Nodes.Num();
			CachedThreshold.SetNumUninitialized(NumNodes);
			for (const PCGExCluster::FNode& Node : CapturedCluster->Nodes) { CachedThreshold[Node.NodeIndex] = Adjacency.GetThreshold(Node); }
			*/
		}

		return false;
	}

	bool TEdgeDirectionFilter::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const FPCGPoint& Point = CapturedCluster->PointsIO->GetInPoint(Node.PointIndex);

		const FVector RefDir = TypedFilterFactory->Descriptor.bTransformDirection ?
			                       Point.Transform.TransformVectorNoScale(OperandDirection->Values[Node.PointIndex].GetSafeNormal()) :
			                       OperandDirection->Values[Node.PointIndex].GetSafeNormal();
		const double A = DotComparison.GetDot(Point);
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
					FVector Direction = (CapturedCluster->Nodes[PCGEx::H64A(Node.Adjacency[i])].Position - Node.Position).GetSafeNormal();
					Dots[i] = FVector::DotProduct(RefDir, Direction);
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (CapturedCluster->Nodes[PCGEx::H64A(Node.Adjacency[i])].Position - Node.Position).GetSafeNormal();
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Direction));
				}
			}
		}
		else
		{
			if (DotComparison.bUnsignedDot)
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (Node.Position - CapturedCluster->Nodes[PCGEx::H64A(Node.Adjacency[i])].Position).GetSafeNormal();
					Dots[i] = FVector::DotProduct(RefDir, Direction);
				}
			}
			else
			{
				for (int i = 0; i < Dots.Num(); i++)
				{
					FVector Direction = (Node.Position - CapturedCluster->Nodes[PCGEx::H64A(Node.Adjacency[i])].Position).GetSafeNormal();
					Dots[i] = FMath::Abs(FVector::DotProduct(RefDir, Direction));
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

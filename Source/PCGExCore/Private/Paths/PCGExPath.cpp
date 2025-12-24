// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPath.h"

#include "GeomTools.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Math/PCGExMathAxis.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Paths/PCGExPathIntersectionDetails.h"

#if PCGEX_ENGINE_VERSION > 506
#include "Data/PCGPolygon2DData.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExPaths"
#define PCGEX_NAMESPACE PCGExPaths

namespace PCGExPaths
{
	FPathEdge::FPathEdge(const int32 InStart, const int32 InEnd, const TConstPCGValueRange<FTransform>& Positions, const double Expansion)
		: Start(InStart), End(InEnd), AltStart(InStart)
	{
		Update(Positions, Expansion);
	}

	void FPathEdge::Update(const TConstPCGValueRange<FTransform>& Positions, const double Expansion)
	{
		PCGEX_SET_BOX_TOLERANCE(Bounds, Positions[Start].GetLocation(), Positions[End].GetLocation(), Expansion);
		Dir = (Positions[End].GetLocation() - Positions[Start].GetLocation()).GetSafeNormal();
	}

	bool FPathEdge::ShareIndices(const FPathEdge& Other) const
	{
		return Start == Other.Start || Start == Other.End || End == Other.Start || End == Other.End;
	}

	bool FPathEdge::Connects(const FPathEdge& Other) const
	{
		return Start == Other.End || End == Other.Start;
	}

	bool FPathEdge::ShareIndices(const FPathEdge* Other) const
	{
		return Start == Other->Start || Start == Other->End || End == Other->Start || End == Other->End;
	}

	double FPathEdge::GetLength(const TConstPCGValueRange<FTransform>& Positions) const
	{
		return FVector::Dist(Positions[Start].GetLocation(), Positions[End].GetLocation());
	}

	void IPathEdgeExtra::ProcessingDone(const FPath* Path)
	{
	}

	FPath::FPath(const bool IsClosed)
		: bClosedLoop(IsClosed)
	{
	}

	FPath::FPath(const TConstPCGValueRange<FTransform>& InTransforms, const bool IsClosed, const double Expansion)
		: FPath(IsClosed)
	{
		Positions = InTransforms;
		NumPoints = InTransforms.Num();
		LastIndex = NumPoints - 1;

		BuildPath(Expansion);
	}

	FPath::FPath(const UPCGBasePointData* InPointData, const double Expansion)
		: FPath(InPointData->GetConstTransformValueRange(), Helpers::GetClosedLoop(InPointData), Expansion)
	{
	}

	PCGExMT::FScope FPath::GetEdgeScope(const int32 InLoopIndex) const
	{
		return PCGExMT::FScope(0, NumEdges, InLoopIndex);
	}

	int32 FPath::LoopPointIndex(const int32 Index) const
	{
		const int32 W = Index % NumPoints;
		return W < 0 ? W + NumPoints : W;
	}

	int32 FPath::SafePointIndex(const int32 Index) const
	{
		if (bClosedLoop) { return PCGExMath::Tile(Index, 0, LastIndex); }
		return Index < 0 ? 0 : Index > LastIndex ? LastIndex : Index;
	}

	FVector FPath::DirToNextPoint(const int32 Index) const
	{
		if (bClosedLoop) { return Edges[Index].Dir; }
		return Index == LastIndex ? Edges[Index - 1].Dir : Edges[Index].Dir;
	}

	FVector FPath::DirToNeighbor(const int32 Index, const int32 Offset) const
	{
		if (Offset < 0) { return DirToPrevPoint(Index); }
		return DirToNextPoint(Index);
	}

	PCGExMath::FClosestPosition FPath::FindClosestIntersection(const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& Segment) const
	{
		PCGExMath::FClosestPosition Closest(Segment.A);

		if (!Bounds.Intersect(Segment.Bounds)) { return Closest; }

		const uint8 Strictness = InDetails.Strictness;

		GetEdgeOctree()->FindElementsWithBoundsTest(Segment.Bounds, [&](const FPathEdge* PathEdge)
		{
			if (InDetails.bWantsDotCheck)
			{
				if (!InDetails.CheckDot(FMath::Abs(Segment.Dot(PathEdge->Dir)))) { return; }
			}

			FVector OnSegment = FVector::ZeroVector;
			FVector OnPath = FVector::ZeroVector;

			if (!Segment.FindIntersection(GetPos_Unsafe(PathEdge->Start), GetPos_Unsafe(PathEdge->End), InDetails.ToleranceSquared, OnSegment, OnPath, Strictness))
			{
				return;
			}

			Closest.Update(OnPath, PathEdge->Start);
		});

		return Closest;
	}

	PCGExMath::FClosestPosition FPath::FindClosestIntersection(const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& Segment, PCGExMath::FClosestPosition& OutClosestPosition) const
	{
		PCGExMath::FClosestPosition Closest(Segment.A);

		if (!Bounds.Intersect(Segment.Bounds)) { return Closest; }

		const uint8 Strictness = InDetails.Strictness;

		GetEdgeOctree()->FindElementsWithBoundsTest(Segment.Bounds, [&](const FPathEdge* PathEdge)
		{
			if (InDetails.bWantsDotCheck)
			{
				if (!InDetails.CheckDot(FMath::Abs(Segment.Dot(PathEdge->Dir)))) { return; }
			}

			FVector OnSegment = FVector::ZeroVector;
			FVector OnPath = FVector::ZeroVector;

			if (!Segment.FindIntersection(GetPos_Unsafe(PathEdge->Start), GetPos_Unsafe(PathEdge->End), InDetails.ToleranceSquared, OnSegment, OnPath, Strictness))
			{
				OutClosestPosition.Update(OnPath, -2);
				return;
			}

			OutClosestPosition.Update(OnPath, -2);
			Closest.Update(OnPath, PathEdge->Start);
		});

		return Closest;
	}

	void FPath::BuildEdgeOctree()
	{
		if (EdgeOctree) { return; }
		EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		for (FPathEdge& Edge : Edges)
		{
			if (!IsEdgeValid(Edge)) { continue; } // Skip zero-length edges
			EdgeOctree->AddElement(&Edge);        // Might be a problem if edges gets reallocated
		}
	}

	void FPath::BuildPartialEdgeOctree(const TArray<int8>& Filter)
	{
		if (EdgeOctree) { return; }
		EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		for (int i = 0; i < Edges.Num(); i++)
		{
			FPathEdge& Edge = Edges[i];
			if (!Filter[i] || !IsEdgeValid(Edge)) { continue; } // Skip filtered out & zero-length edges
			EdgeOctree->AddElement(&Edge);                      // Might be a problem if edges gets reallocated
		}
	}

	void FPath::BuildPartialEdgeOctree(const TBitArray<>& Filter)
	{
		if (EdgeOctree) { return; }
		EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		for (int i = 0; i < Edges.Num(); i++)
		{
			FPathEdge& Edge = Edges[i];
			if (!Filter[i] || !IsEdgeValid(Edge)) { continue; } // Skip filtered out & zero-length edges
			EdgeOctree->AddElement(&Edge);                      // Might be a problem if edges gets reallocated
		}
	}

	void FPath::UpdateConvexity(const int32 Index)
	{
		if (!bIsConvex) { return; }

		const int32 A = SafePointIndex(Index - 1);
		const int32 B = SafePointIndex(Index + 1);
		if (A == B)
		{
			bIsConvex = false;
			return;
		}

		PCGExMath::CheckConvex(Positions[A].GetLocation(), Positions[Index].GetLocation(), Positions[B].GetLocation(), bIsConvex, ConvexitySign);
	}

	void FPath::ComputeEdgeExtra(const int32 Index)
	{
		if (NumEdges == 1)
		{
			for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
		}
		else
		{
			if (Index == 0) { for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); } }
			else if (Index == LastEdge) { for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); } }
			else { for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessEdge(this, Edges[Index]); } }
		}
	}

	void FPath::ExtraComputingDone()
	{
		for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessingDone(this); }
		Extras.Empty(); // So we don't update them anymore
	}

	void FPath::ComputeAllEdgeExtra()
	{
		if (NumEdges == 1)
		{
			for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
		}
		else
		{
			for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); }
			for (int i = 1; i < LastEdge; i++) { for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessEdge(this, Edges[i]); } }
			for (const TSharedPtr<IPathEdgeExtra>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); }
		}

		ExtraComputingDone();
	}

	bool FPath::IsInsideProjection(const FVector& WorldPosition) const
	{
		const FVector2D ProjectedPoint = FVector2D(Projection.ProjectFlat(WorldPosition));
		if (!ProjectedBounds.IsInside(ProjectedPoint)) { return false; }
		return FGeomTools2D::IsPointInPolygon(ProjectedPoint, ProjectedPoints);
	}

	bool FPath::Contains(const TConstPCGValueRange<FTransform>& InPositions, const double Tolerance) const
	{
		const int32 OtherNumPoints = InPositions.Num();
		const int32 Threshold = FMath::Min(1, FMath::RoundToInt(static_cast<double>(OtherNumPoints) * (1 - FMath::Clamp(Tolerance, 0, 1))));

		int32 InsideCount = 0;

		for (int i = 0; i < OtherNumPoints; i++)
		{
			if (IsInsideProjection(InPositions[i].GetLocation()))
			{
				InsideCount++;
				if (InsideCount >= Threshold) { return true; }
			}
		}

		return false;
	}

	void FPath::BuildProjection()
	{
		ProjectedPoints.SetNumUninitialized(NumPoints);
		ProjectedBounds = FBox2D();

		for (int i = 0; i < NumPoints; i++)
		{
			const FVector2D ProjectedPoint = FVector2D(Projection.ProjectFlat(GetPos_Unsafe(i), i));
			ProjectedBounds += ProjectedPoint;
			ProjectedPoints[i] = ProjectedPoint;
		}
	}

	void FPath::BuildProjection(const FPCGExGeo2DProjectionDetails& InProjectionDetails)
	{
		Projection = InProjectionDetails;
		BuildProjection();
	}

	void FPath::OffsetProjection(const double Offset)
	{
		if (FMath::IsNearlyZero(Offset)) { return; }

		if (Offset > 0) { ProjectedBounds = ProjectedBounds.ExpandBy(Offset); }

		const int32 N = ProjectedPoints.Num();
		if (N < 3) { return; }

		TArray<FVector2D> InsetPositions;
		InsetPositions.SetNum(N);

		ProjectedBounds = FBox2D();

		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = ProjectedPoints[(i - 1 + N) % N];
			const FVector2D& B = ProjectedPoints[i];
			const FVector2D& C = ProjectedPoints[(i + 1) % N];

			const FVector2D AB = (B - A).GetSafeNormal();
			const FVector2D BC = (C - B).GetSafeNormal();

			const FVector2D N1 = FVector2D(-AB.Y, AB.X);
			const FVector2D N2 = FVector2D(-BC.Y, BC.X);

			const FVector2D Avg = (N1 + N2).GetSafeNormal();

			const FVector2D Pos = B - Avg * Offset;
			InsetPositions[i] = Pos;
			ProjectedBounds += Pos;
		}

		ProjectedPoints.Empty();
		ProjectedPoints = MoveTemp(InsetPositions);
	}

	void FPath::BuildPath(const double Expansion)
	{
		if (bClosedLoop) { NumEdges = NumPoints; }
		else { NumEdges = LastIndex; }

		LastEdge = NumEdges - 1;

		Edges.SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const FPathEdge& E = (Edges[i] = FPathEdge(i, (i + 1) % NumPoints, Positions, Expansion));
			TotalLength += E.GetLength(Positions);
			Bounds += E.Bounds.GetBox();
		}
	}

#pragma region Edge extras

#pragma region FPathEdgeLength

	void FPathEdgeLength::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::Dist(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
		TotalLength += Dist;
	}

	void FPathEdgeLength::ProcessingDone(const FPath* Path)
	{
		TPathEdgeExtra<double>::ProcessingDone(Path);
		CumulativeLength.SetNumUninitialized(Data.Num());
		CumulativeLength[0] = Data[0];
		for (int i = 1; i < Data.Num(); i++) { CumulativeLength[i] = CumulativeLength[i - 1] + Data[i]; }
	}

#pragma endregion

#pragma region FPathEdgeLength

	void FPathEdgeLengthSquared::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::DistSquared(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
	}

#pragma endregion

#pragma region FPathEdgeNormal

	void FPathEdgeNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeBinormal

	void FPathEdgeBinormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		Normals[Edge.Start] = N;
		GetMutable(Edge.Start) = N;
	}

	void FPathEdgeBinormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		Normals[Edge.Start] = N;

		const FVector A = Path->DirToPrevPoint(Edge.Start);
		FVector D = FQuat(FVector::CrossProduct(A, Edge.Dir).GetSafeNormal(), FMath::Acos(FVector::DotProduct(A, Edge.Dir)) * 0.5f).RotateVector(A);

		if (FVector::DotProduct(N, D) < 0.0f) { D *= -1; }

		GetMutable(Edge.Start) = D;
	}

#pragma endregion

#pragma region FPathEdgeAvgNormal

	void FPathEdgeAvgNormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

	void FPathEdgeAvgNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector A = FVector::CrossProduct(Up, Path->DirToPrevPoint(Edge.Start) * -1).GetSafeNormal();
		const FVector B = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		GetMutable(Edge.Start) = FMath::Lerp(A, B, 0.5).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeHalfAngle

	void FPathEdgeHalfAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	void FPathEdgeHalfAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FMath::Acos(FVector::DotProduct(Path->DirToPrevPoint(Edge.Start), Edge.Dir));
	}

#pragma endregion

#pragma region FPathEdgeAngle

	void FPathEdgeFullAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = 0;
	}

	void FPathEdgeFullAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = PCGExMath::GetAngle(Path->DirToPrevPoint(Edge.Start) * -1, Edge.Dir);
	}

#pragma endregion
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

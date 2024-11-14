// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeoPrimtives.h"
#include "Graph/PCGExCluster.h"

namespace PCGExTopology
{
	enum class ETriangulationResult : uint8
	{
		Unknown = 0,
		Success,
		InvalidCell,
		TooFewPoints,
		UnsupportedAspect,
		InvalidCluster,
	};

	enum class ECellResult : uint8
	{
		Unknown = 0,
		Success,
		Duplicate,
		DeadEnd,
		WrongAspect,
		ExceedPointsLimit,
		BelowPointsLimit,
		ExceedBoundsLimit,
		BelowBoundsLimit,
		OpenCell,
	};

	const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");

	static FORCEINLINE void MarkTriangle(
		const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		const PCGExGeo::FTriangle& InTriangle)
	{
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[0])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[1])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[2])->bValid, 1);
	}

	class FCell;

	class FCellConstraints : public TSharedFromThis<FCellConstraints>
	{
	public:
		mutable FRWLock UniquePathsBoxHashLock;
		mutable FRWLock UniquePathsStartHashLock;
		TSet<uint32> UniquePathsBoxHash;
		TSet<uint64> UniquePathsStartHash;

		bool bConcaveOnly = false;
		bool bConvexOnly = false;
		bool bKeepContoursWithDeadEnds = true;
		bool bDuplicateDeadEndPoints = false;
		bool bClosedLoopOnly = false;

		int32 MaxPointCount = MAX_int32;
		int32 MinPointCount = MIN_int32;

		double MaxBoundsSize = MAX_dbl;
		double MinBoundsSize = MIN_dbl;

		bool bDedupe = true;

		FCellConstraints()
		{
		}

		bool ContainsSignedEdgeHash(const uint64 Hash) const;
		bool IsUniqueStartHash(const uint64 Hash);
		bool IsUniqueCellHash(const FCell* InCell);
	};

	class FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;

	public:
		TArray<int32> Nodes;
		FBox Bounds = FBox(ForceInit);
		TSharedRef<FCellConstraints> Constraints;

		bool bIsConvex = true;
		bool bCompiledSuccessfully = false;
		bool bIsClosedLoop = false;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
		}

		~FCell() = default;

		ECellResult BuildFromCluster(
			const int32 SeedNodeIndex,
			const int32 SeedEdgeIndex,
			const FVector& Guide,
			TSharedRef<PCGExCluster::FCluster> InCluster,
			const TArray<FVector>& ProjectedPositions,
			TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes);

		ECellResult BuildFromPath(
			const TArray<FVector>& ProjectedPositions);

		template <bool bMarkTriangles = false>
		ETriangulationResult Triangulate(
			const TArray<FVector>& ProjectedPositions,
			TArray<PCGExGeo::FTriangle>& OutTriangles,
			const TSharedPtr<PCGExCluster::FCluster> InCluster = nullptr)
		{
			if constexpr (bMarkTriangles) { if (!InCluster) { return ETriangulationResult::InvalidCluster; } }
			if (!bCompiledSuccessfully) { return ETriangulationResult::InvalidCell; }
			if (Nodes.Num() < 3) { return ETriangulationResult::TooFewPoints; }
			if (bIsConvex) { return TriangulateFan<bMarkTriangles>(ProjectedPositions, OutTriangles, InCluster); }
			else { return TriangulateEarClipping<bMarkTriangles>(ProjectedPositions, OutTriangles, InCluster); }
		}

		int32 GetTriangleNumEstimate() const;

	protected:
		template <bool bMarkTriangles = false>
		ETriangulationResult TriangulateFan(
			TArray<PCGExGeo::FTriangle>& OutTriangles,
			const TSharedPtr<PCGExCluster::FCluster> InCluster = nullptr)
		{
			if (!bCompiledSuccessfully) { return ETriangulationResult::InvalidCell; }
			if (!bIsConvex) { return ETriangulationResult::UnsupportedAspect; }
			if (Nodes.Num() < 3) { return ETriangulationResult::TooFewPoints; }
			for (int i = 1; i < Nodes.Num(); i++)
			{
				const PCGExGeo::FTriangle& T = OutTriangles.Emplace_GetRef(Nodes[0], Nodes[i], Nodes[i + 1]);
				if constexpr (bMarkTriangles) { MarkTriangle(InCluster, T); }
			}

			return ETriangulationResult::Success;
		}

		template <bool bMarkTriangles = false>
		ETriangulationResult TriangulateEarClipping(
			const TArray<FVector>& ProjectedPositions,
			TArray<PCGExGeo::FTriangle>& OutTriangles,
			const TSharedPtr<PCGExCluster::FCluster> InCluster = nullptr)
		{
			if (!bCompiledSuccessfully) { return ETriangulationResult::InvalidCell; }

			int32 NumNodes = Nodes.Num();
			if (NumNodes < 3) { return ETriangulationResult::TooFewPoints; }

			TArray<int32> NodeQueue;
			PCGEx::ArrayOfIndices(NodeQueue, NumNodes);

			int32 PrevIndex = NumNodes - 1;
			int32 CurrIndex = 0;
			int32 NextIndex = 1;

			while (NodeQueue.Num() > 2)
			{
				bool bEarFound = false;

				for (int32 i = 0; i < NodeQueue.Num(); i++)
				{
					int32 AIdx = NodeQueue[PrevIndex];
					int32 BIdx = NodeQueue[CurrIndex];
					int32 CIdx = NodeQueue[NextIndex];

					FVector A = ProjectedPositions[AIdx];
					FVector B = ProjectedPositions[BIdx];
					FVector C = ProjectedPositions[CIdx];

					FBox TBox = FBox(ForceInit);
					TBox += A;
					TBox += B;
					TBox += C;

					// Check if triangle ABC is an ear
					bool bIsEar = true;

					for (int32 j = 0; j < NodeQueue.Num(); j++)
					{
						if (j == AIdx || j == BIdx || j == CIdx) { continue; }

						const FVector& P = ProjectedPositions[NodeQueue[j]];
						if (!TBox.IsInside(P)) { continue; }

						if (PCGExGeo::IsPointInTriangle(P, A, B, C))
						{
							bIsEar = false;
							break;
						}
					}

					if (bIsEar)
					{
						// Add the ear triangle to the result
						const PCGExGeo::FTriangle& T = OutTriangles.Emplace_GetRef(Nodes[0], Nodes[i], Nodes[i + 1]);
						if constexpr (bMarkTriangles) { MarkTriangle(InCluster, T); }

						NodeQueue.RemoveAtSwap(CurrIndex);
						NumNodes = NodeQueue.Num();
						PrevIndex = (CurrIndex - 1 + NumNodes) % NumNodes;
						CurrIndex = CurrIndex % NumNodes;
						NextIndex = (CurrIndex + 1) % NumNodes;

						bEarFound = true;
						break;
					}

					PrevIndex = CurrIndex;
					CurrIndex = NextIndex;
					NextIndex = (NextIndex + 1) % NodeQueue.Num();
				}

				if (!bEarFound) { return ETriangulationResult::InvalidCell; }
			}

			return ETriangulationResult::Success;
		}
	};
}

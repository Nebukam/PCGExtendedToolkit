// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Containers/PCGExScopedContainers.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Clusters/PCGExEdge.h"
#include "Clusters/Artifacts/PCGExCellDetails.h"
#include "Math/PCGExProjectionDetails.h"
#include "Math/PCGExWinding.h"

class UPCGBasePointData;
struct FPCGExCellConstraintsDetails;

namespace PCGExData
{
	struct FMutablePoint;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExMath
{
	struct FTriangle;
}

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExClusters
{
	namespace Labels
	{
		const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");
		const FName SourceHolesLabel = FName("Holes");
	}

	PCGEXCORE_API
	void SetPointProperty(PCGExData::FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty);

#pragma region Cell

	enum class ECellResult : uint8
	{
		Unknown = 0,
		Success,
		Duplicate,
		Leaf,
		Hole,
		WrongAspect,
		OutsidePointsLimit,
		OutsideBoundsLimit,
		OutsideAreaLimit,
		OutsidePerimeterLimit,
		OutsideCompactnessLimit,
		OutsideSegmentsLimit,
		OpenCell,
		WrapperCell,
		MalformedCluster,
	};

	class FCell;
	class FCellConstraints;
	struct FNode;

	class PCGEXCORE_API FHoles : public TSharedFromThis<FHoles>
	{
	protected:
		mutable FRWLock ProjectionLock;

		TSharedRef<PCGExData::FFacade> PointDataFacade;
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector2D> ProjectedPoints;

	public:
		explicit FHoles(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const FPCGExGeo2DProjectionDetails& InProjectionDetails)
			: PointDataFacade(InPointDataFacade), ProjectionDetails(InProjectionDetails)
		{
			if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { ProjectionDetails.Init(PointDataFacade); }
		}

		bool Overlaps(const TArray<FVector2D>& Polygon);
	};

#pragma region DCEL Face Enumeration

	/**
	 * Half-edge structure for DCEL-based planar face enumeration.
	 * Each undirected edge becomes two half-edges pointing in opposite directions.
	 */
	struct PCGEXCORE_API FHalfEdge
	{
		int32 OriginNode = -1;    // Node index where this half-edge starts
		int32 TargetNode = -1;    // Node index where this half-edge ends
		int32 TwinIndex = -1;     // Index of the opposite half-edge
		int32 NextIndex = -1;     // Index of the next half-edge in the face (CCW)
		int32 FaceIndex = -1;     // Index of the face this half-edge bounds (-1 if not yet assigned)
		double Angle = 0;         // Angle of this half-edge from origin (for sorting)

		FHalfEdge() = default;
		FHalfEdge(int32 InOrigin, int32 InTarget, double InAngle)
			: OriginNode(InOrigin), TargetNode(InTarget), Angle(InAngle)
		{
		}
	};

	/**
	 * DCEL-based planar face enumerator.
	 * Builds a proper half-edge structure and enumerates all faces by following next pointers.
	 */
	class PCGEXCORE_API FPlanarFaceEnumerator
	{
	protected:
		TArray<FHalfEdge> HalfEdges;
		TMap<uint64, int32> HalfEdgeMap;  // Maps H64(origin, target) to half-edge index

		const FCluster* Cluster = nullptr;
		const TArray<FVector2D>* ProjectedPositions = nullptr;

		int32 NumFaces = 0;

	public:
		FPlanarFaceEnumerator() = default;

		/**
		 * Build the DCEL structure from a cluster.
		 * @param InCluster The cluster to build from
		 * @param InProjectedPositions 2D projected positions indexed by point index
		 */
		void Build(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& InProjectedPositions);

		/**
		 * Enumerate all faces and create cells.
		 * @param OutCells Output array of cells (faces that pass constraints)
		 * @param Constraints Cell constraints for filtering
		 */
		void EnumerateAllFaces(TArray<TSharedPtr<FCell>>& OutCells, const TSharedRef<FCellConstraints>& Constraints);

		/**
		 * Find the face containing a given 2D point.
		 * @param Point The 2D point to test
		 * @return Face index, or -1 if not found
		 */
		int32 FindFaceContaining(const FVector2D& Point) const;

		/**
		 * Get the outer (wrapper) face index.
		 * This is the unbounded face surrounding the entire graph.
		 * @return Index of the wrapper face, or -1 if not found
		 */
		int32 GetWrapperFaceIndex() const;

		FORCEINLINE bool IsBuilt() const { return !HalfEdges.IsEmpty(); }
		FORCEINLINE int32 GetNumHalfEdges() const { return HalfEdges.Num(); }
		FORCEINLINE int32 GetNumFaces() const { return NumFaces; }

		/**
		 * Get half-edge index for a directed edge.
		 * @return Half-edge index, or -1 if not found
		 */
		FORCEINLINE int32 GetHalfEdgeIndex(int32 FromNode, int32 ToNode) const
		{
			const int32* Found = HalfEdgeMap.Find(PCGEx::H64(FromNode, ToNode));
			return Found ? *Found : -1;
		}

	protected:
		/** Build a cell from a face (list of node indices) */
		ECellResult BuildCellFromFace(
			const TArray<int32>& FaceNodes,
			TSharedPtr<FCell>& OutCell,
			const TSharedRef<FCellConstraints>& Constraints) const;
	};

#pragma endregion

	class PCGEXCORE_API FCellConstraints : public TSharedFromThis<FCellConstraints>
	{
	protected:
		PCGExMT::TH64SetShards<> UniquePathsHashSet;
		PCGExMT::TH64SetShards<> UniqueStartHalfEdgesHash;

	public:
		EPCGExWinding Winding = EPCGExWinding::CounterClockwise;

		bool bConcaveOnly = false;
		bool bConvexOnly = false;
		bool bKeepCellsWithLeaves = true;
		bool bDuplicateLeafPoints = false;

		int32 MaxPointCount = MAX_int32;
		int32 MinPointCount = MIN_int32;

		double MaxBoundsSize = MAX_dbl;
		double MinBoundsSize = MIN_dbl_neg;

		double MaxArea = MAX_dbl;
		double MinArea = MIN_dbl_neg;

		double MaxPerimeter = MAX_dbl;
		double MinPerimeter = MIN_dbl_neg;

		double MaxSegmentLength = MAX_dbl;
		double MinSegmentLength = MIN_dbl_neg;

		double MaxCompactness = MAX_dbl;
		double MinCompactness = MIN_dbl_neg;

		double WrapperClassificationTolerance = 0;
		bool bBuildWrapper = true;

		TSharedPtr<FCell> WrapperCell;
		TSharedPtr<FHoles> Holes;

		FCellConstraints()
		{
		}

		explicit FCellConstraints(const FPCGExCellConstraintsDetails& InDetails);

		void Reserve(const int32 InCellHashReserve);
		bool ContainsSignedEdgeHash(const uint64 Hash);
		bool IsUniqueStartHalfEdge(const uint64 Hash);
		bool IsUniqueCellHash(const TSharedPtr<FCell>& InCell);
		void BuildWrapperCell(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints = nullptr);

		void Cleanup();
	};

	struct PCGEXCORE_API FCellData
	{
		int8 bIsValid = 0;
		uint32 CellHash = 0;
		FBox Bounds = FBox(ForceInit);
		FVector Centroid = FVector::ZeroVector;
		double Area = 0;
		double Perimeter = 0;
		double Compactness = 0;
		bool bIsConvex = true;
		bool bIsClockwise = false;
		bool bIsClosedLoop = false;

		FCellData() = default;
	};

	class PCGEXCORE_API FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;
		uint64 CellHash = 0;

	public:
		TArray<int32> Nodes;
		TSharedRef<FCellConstraints> Constraints;

		FCellData Data = FCellData();

		PCGExGraphs::FLink Seed = PCGExGraphs::FLink(-1, -1);

		bool bBuiltSuccessfully = false;

		TArray<FVector2D> Polygon;

		int32 CustomIndex = -1;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
			Data.bIsValid = true;
		}

		~FCell() = default;

		uint64 GetCellHash();

		/** Build cell from a half-edge using on-the-fly angle calculation (original method) */
		ECellResult BuildFromCluster(const PCGExGraphs::FLink InSeedLink, TSharedRef<FCluster> InCluster, const TArray<FVector2D>& ProjectedPositions);

		/** Build cell from seed position using on-the-fly angle calculation (original method) */
		ECellResult BuildFromCluster(const FVector& SeedPosition, const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FPCGExNodeSelectionDetails* Picking = nullptr);

		ECellResult BuildFromPath(const TArray<FVector2D>& ProjectedPositions);

		void PostProcessPoints(UPCGBasePointData* InMutablePoints);
	};

#pragma endregion
}

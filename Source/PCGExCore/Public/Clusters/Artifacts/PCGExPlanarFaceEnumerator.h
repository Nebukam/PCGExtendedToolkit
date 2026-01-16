// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExH.h"

namespace PCGExClusters
{
	class FCluster;
	class FCell;
	class FCellConstraints;
	enum class ECellResult : uint8;

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
	 * Raw face data - lightweight structure for parallel cell building
	 */
	struct PCGEXCORE_API FRawFace
	{
		TArray<int32> Nodes;
		int32 FaceIndex = -1;

		FRawFace() = default;
		explicit FRawFace(int32 InFaceIndex) : FaceIndex(InFaceIndex) {}
	};

	/**
	 * DCEL-based planar face enumerator.
	 * Builds a proper half-edge structure and enumerates all faces by following next pointers.
	 */
	class PCGEXCORE_API FPlanarFaceEnumerator : public TSharedFromThis<FPlanarFaceEnumerator>
	{
	protected:
		TArray<FHalfEdge> HalfEdges;
		TMap<uint64, int32> HalfEdgeMap;  // Maps H64(origin, target) to half-edge index

		const FCluster* Cluster = nullptr;
		const TArray<FVector2D>* ProjectedPositions = nullptr;

		int32 NumFaces = 0;

		// Cached raw faces for reuse
		TArray<FRawFace> CachedRawFaces;
		bool bRawFacesEnumerated = false;

	public:
		FPlanarFaceEnumerator() = default;

		/**
		 * Build the DCEL structure from a cluster.
		 * @param InCluster The cluster to build from
		 * @param InProjectedPositions 2D projected positions indexed by point index
		 */
		void Build(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& InProjectedPositions);

		/**
		 * Enumerate raw faces (serial operation).
		 * Call this once, then use BuildCellsFromRawFaces for parallel cell building.
		 * @return Reference to cached raw faces
		 */
		const TArray<FRawFace>& EnumerateRawFaces();

		/**
		 * Build cells from raw faces. Can be called in parallel per-face.
		 * @param InRawFace The raw face data
		 * @param OutCell Output cell (caller should allocate)
		 * @param Constraints Cell constraints for filtering
		 * @return Cell result status
		 */
		ECellResult BuildCellFromRawFace(
			const FRawFace& InRawFace,
			TSharedPtr<FCell>& OutCell,
			const TSharedRef<FCellConstraints>& Constraints) const;

		/**
		 * Enumerate all faces and create cells (convenience method, combines EnumerateRawFaces + BuildCellFromRawFace).
		 * @param OutCells Output array of cells (faces that pass constraints)
		 * @param Constraints Cell constraints for filtering
		 * @param OutFailedCells Optional output array of cells that failed constraints (but have valid polygons for containment testing)
		 */
		void EnumerateAllFaces(TArray<TSharedPtr<FCell>>& OutCells, const TSharedRef<FCellConstraints>& Constraints, TArray<TSharedPtr<FCell>>* OutFailedCells = nullptr);

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
		FORCEINLINE const FCluster* GetCluster() const { return Cluster; }
		FORCEINLINE const TArray<FVector2D>* GetProjectedPositions() const { return ProjectedPositions; }

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
		/** Build a cell from a face (list of node indices) - internal use */
		ECellResult BuildCellFromFace(
			const TArray<int32>& FaceNodes,
			TSharedPtr<FCell>& OutCell,
			const TSharedRef<FCellConstraints>& Constraints) const;
	};
}

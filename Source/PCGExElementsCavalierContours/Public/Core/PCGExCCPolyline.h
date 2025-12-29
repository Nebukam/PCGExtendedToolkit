// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"
#include "PCGExCCMath.h"

struct FPCGExCCArcTessellationSettings;

namespace PCGExCavalier
{
	/**
	 * A 2D polyline consisting of vertices connected by line segments or arcs.
	 * 
	 * Each vertex has a bulge value that determines the curvature of the segment
	 * from that vertex to the next. A bulge of 0 creates a straight line segment,
	 * while non-zero bulge values create circular arcs.
	 * 
	 * Polylines can be open or closed. For closed polylines, the last vertex
	 * connects back to the first vertex.
	 * 
	 * Each polyline can track which root paths contributed to it via ContributingPathIds.
	 * For polylines created directly from input, PrimaryPathId stores the single source.
	 * For boolean operation results, multiple paths may contribute.
	 */
	class PCGEXELEMENTSCAVALIERCONTOURS_API FPolyline
	{
	public:
		using FSegmentVisitor = TFunction<void(const FVertex&, const FVertex&)>;
		using FSegmentVisitorIndexed = TFunction<void(int32, const FVertex&, const FVertex&)>;

		/**
		 * Simple AABB spatial index for efficient segment queries.
		 * Each entry stores the bounding box of a segment.
		 */
		struct FApproxAABBIndex
		{
			struct FBox
			{
				double MinX, MinY, MaxX, MaxY;

				bool Overlaps(const FBox& Other) const
				{
					return !(MaxX < Other.MinX || MinX > Other.MaxX || MaxY < Other.MinY || MinY > Other.MaxY);
				}

				bool Overlaps(double QMinX, double QMinY, double QMaxX, double QMaxY) const
				{
					return !(MaxX < QMinX || MinX > QMaxX || MaxY < QMinY || MinY > QMaxY);
				}
			};

			TArray<FBox> Boxes;

			/** Query all segments whose AABB overlaps the given region */
			template <typename Visitor>
			void Query(double MinX, double MinY, double MaxX, double MaxY, Visitor&& Visit) const
			{
				for (int32 i = 0; i < Boxes.Num(); ++i)
				{
					if (Boxes[i].Overlaps(MinX, MinY, MaxX, MaxY)) { Visit(i); }
				}
			}

			/** Query all segments whose AABB overlaps the given box */
			template <typename Visitor>
			void Query(const FBox& QueryBox, Visitor&& Visit) const
			{
				Query(QueryBox.MinX, QueryBox.MinY, QueryBox.MaxX, QueryBox.MaxY, Forward<Visitor>(Visit));
			}
		};

	private:
		TArray<FVertex> Vertices;
		bool bClosed = false;

		/** Primary path ID for single-source polylines (e.g., from input or offset) */
		int32 PrimaryPathId = INDEX_NONE;

		/** All path IDs that contributed to this polyline (for boolean results) */
		TSet<int32> ContributingPathIds;

	public:
		FPolyline() = default;

		explicit FPolyline(bool bInClosed)
			: bClosed(bInClosed)
		{
		}

		FPolyline(bool bInClosed, int32 InPrimaryPathId)
			: bClosed(bInClosed)
			  , PrimaryPathId(InPrimaryPathId)
		{
			if (InPrimaryPathId != INDEX_NONE)
			{
				ContributingPathIds.Add(InPrimaryPathId);
			}
		}

		// Path Tracking

		/** Get the primary path ID (for single-source polylines) */
		int32 GetPrimaryPathId() const { return PrimaryPathId; }

		/** Set the primary path ID */
		void SetPrimaryPathId(int32 PathId)
		{
			PrimaryPathId = PathId;
			if (PathId != INDEX_NONE)
			{
				ContributingPathIds.Add(PathId);
			}
		}

		/** Get all path IDs that contributed to this polyline */
		const TSet<int32>& GetContributingPathIds() const { return ContributingPathIds; }

		/** Add a contributing path ID */
		void AddContributingPath(int32 PathId)
		{
			if (PathId != INDEX_NONE)
			{
				ContributingPathIds.Add(PathId);
			}
		}

		/** Add multiple contributing path IDs */
		void AddContributingPaths(const TSet<int32>& PathIds)
		{
			ContributingPathIds.Append(PathIds);
		}

		/** Check if this polyline has contributions from a specific path */
		bool HasContributionFrom(int32 PathId) const
		{
			return ContributingPathIds.Contains(PathId);
		}

		/** Returns true if this polyline has a single source path */
		bool HasSingleSource() const
		{
			return ContributingPathIds.Num() == 1;
		}

		/** Clear all path tracking information */
		void ClearPathTracking()
		{
			PrimaryPathId = INDEX_NONE;
			ContributingPathIds.Empty();
		}

		/** Collect path IDs from all vertices into ContributingPathIds */
		void CollectPathIdsFromVertices()
		{
			for (const FVertex& V : Vertices)
			{
				if (V.HasValidPath())
				{
					ContributingPathIds.Add(V.GetPathId());
				}
			}
		}

		// Basic Properties

		bool IsClosed() const { return bClosed; }
		void SetClosed(bool bInClosed) { bClosed = bInClosed; }

		int32 VertexCount() const { return Vertices.Num(); }
		bool IsEmpty() const { return Vertices.IsEmpty(); }

		/** Number of segments in the polyline */
		int32 SegmentCount() const
		{
			const int32 N = Vertices.Num();
			if (N < 2)
			{
				return 0;
			}
			return bClosed ? N : N - 1;
		}

		// Vertex Access

		const FVertex& GetVertex(int32 Index) const { return Vertices[Index]; }
		FVertex& GetVertex(int32 Index) { return Vertices[Index]; }

		const FVertex& operator[](int32 Index) const { return Vertices[Index]; }
		FVertex& operator[](int32 Index) { return Vertices[Index]; }

		const FVertex& LastVertex() const { return Vertices.Last(); }
		FVertex& LastVertex() { return Vertices.Last(); }

		const TArray<FVertex>& GetVertices() const { return Vertices; }
		TArray<FVertex>& GetVertices() { return Vertices; }

		/** Get vertex at wrapped index (handles negative and overflow) */
		const FVertex& GetVertexWrapped(int32 Index) const
		{
			const int32 N = Vertices.Num();
			Index = ((Index % N) + N) % N;
			return Vertices[Index];
		}

		/** Compute forward-wrapped index */
		int32 ForwardWrappingIndex(int32 Index, int32 Offset = 1) const
		{
			const int32 N = Vertices.Num();
			return (Index + Offset) % N;
		}

		/** Compute backward-wrapped index */
		int32 BackwardWrappingIndex(int32 Index, int32 Offset = 1) const
		{
			const int32 N = Vertices.Num();
			return ((Index - Offset) % N + N) % N;
		}

		// Vertex Manipulation

		void AddVertex(const FVertex& Vertex)
		{
			Vertices.Add(Vertex);
			if (Vertex.HasValidPath()) { ContributingPathIds.Add(Vertex.GetPathId()); }
		}

		void AddVertex(const FVector2D& Position, double Bulge = 0.0)
		{
			Vertices.Add(FVertex(Position, Bulge));
		}

		void AddVertex(const FVector2D& Position, double Bulge, const FVertexSource& Source)
		{
			FVertex V(Position, Bulge, Source);
			Vertices.Add(V);
			if (Source.HasValidPath()) { ContributingPathIds.Add(Source.PathId); }
		}

		void AddVertex(const FVector2D& Position, double Bulge, int32 PathId, int32 PointIndex)
		{
			AddVertex(Position, Bulge, FVertexSource(PathId, PointIndex));
		}

		void AddVertex(double X, double Y, double Bulge = 0.0)
		{
			Vertices.Add(FVertex(X, Y, Bulge));
		}

		void AddVertex(double X, double Y, double Bulge, const FVertexSource& Source)
		{
			FVertex V(X, Y, Bulge, Source);
			Vertices.Add(V);
			if (Source.HasValidPath()) { ContributingPathIds.Add(Source.PathId); }
		}

		void SetVertex(int32 Index, const FVertex& Vertex)
		{
			Vertices[Index] = Vertex;
			if (Vertex.HasValidPath()) { ContributingPathIds.Add(Vertex.GetPathId()); }
		}

		void RemoveVertex(int32 Index)
		{
			Vertices.RemoveAt(Index);
		}

		void RemoveLastVertex()
		{
			if (!Vertices.IsEmpty()) { Vertices.RemoveAt(Vertices.Num() - 1); }
		}

		void SetLastVertexBulge(double NewBulge)
		{
			if (!Vertices.IsEmpty()) { Vertices.Last().Bulge = NewBulge; }
		}

		void Reserve(int32 Count)
		{
			Vertices.Reserve(Count);
		}

		void Clear()
		{
			Vertices.Empty();
			ClearPathTracking();
		}

		/**
		 * Add or replace a vertex at the end, using fuzzy position matching.
		 * If the last vertex position matches the new position, update it instead of adding.
		 */
		void AddOrReplaceVertex(const FVertex& Vertex, double PosEqualEps = 1e-5)
		{
			if (!Vertices.IsEmpty() && Vertices.Last().PositionEquals(Vertex, PosEqualEps))
			{
				Vertices.Last() = Vertex;
			}
			else
			{
				AddVertex(Vertex);
			}
		}

		// Segment Iteration

		/** Iterate over all segments */
		void ForEachSegment(FSegmentVisitor Visitor) const
		{
			const int32 N = Vertices.Num();
			if (N < 2)
			{
				return;
			}

			const int32 SegCount = bClosed ? N : N - 1;
			for (int32 i = 0; i < SegCount; ++i)
			{
				const int32 NextIdx = (i + 1) % N;
				Visitor(Vertices[i], Vertices[NextIdx]);
			}
		}

		/** Iterate over all segments with index */
		void ForEachSegmentIndexed(FSegmentVisitorIndexed Visitor) const
		{
			const int32 N = Vertices.Num();
			if (N < 2)
			{
				return;
			}

			const int32 SegCount = bClosed ? N : N - 1;
			for (int32 i = 0; i < SegCount; ++i)
			{
				const int32 NextIdx = (i + 1) % N;
				Visitor(i, Vertices[i], Vertices[NextIdx]);
			}
		}

		// Geometric Properties

		/** Compute the signed area of a closed polyline */
		double Area() const;

		/** Compute the total path length */
		double PathLength() const;

		/** Compute axis-aligned bounding box */
		FBox2D BoundingBox() const;

		/** Compute the orientation of a closed polyline */
		EPCGExCCOrientation Orientation() const;

		/** Compute winding number of a point relative to this closed polyline */
		int32 WindingNumber(const FVector2D& Point) const;

		/** Check if point is inside this closed polyline */
		bool ContainsPoint(const FVector2D& Point) const
		{
			return WindingNumber(Point) != 0;
		}

		// Transformations

		/** Reverse the direction of the polyline */
		void Reverse();

		/** Create a reversed copy */
		FPolyline Reversed() const;

		/** Invert the orientation (reverse direction and negate bulges) */
		void InvertOrientation();

		/** Create an inverted copy */
		FPolyline InvertedOrientation() const;

		/** Tessellate all arcs into line segments */
		FPolyline Tessellated(const FPCGExCCArcTessellationSettings& Settings) const;

		// Spatial Index

		/** Create an AABB spatial index for all segments */
		FApproxAABBIndex CreateApproxAABBIndex() const;


		// Closest Point Queries


		/** Find the closest point on the polyline to the given point */
		FVector2D ClosestPoint(const FVector2D& Point, double* OutDistance = nullptr) const;

		/** Find minimum distance to the polyline */
		double DistanceToPoint(const FVector2D& Point) const
		{
			double Dist;
			ClosestPoint(Point, &Dist);
			return Dist;
		}
	};


	/**
	 * Result of 3D contour conversion.
	 * Contains the reconstructed 3D positions and transforms, plus source tracking.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FContourResult3D
	{
		/** 3D positions of all vertices */
		TArray<FVector> Positions;

		/** Full transforms for each vertex (includes rotation/scale from source) */
		TArray<FTransform> Transforms;

		/** Source information for each vertex */
		TArray<FVertexSource> Sources;

		/** All path IDs that contributed to this contour */
		TSet<int32> ContributingPathIds;

		/** Whether this contour is closed */
		bool bIsClosed = false;

		/** Get number of vertices */
		int32 Num() const { return Positions.Num(); }

		/** Check if empty */
		bool IsEmpty() const { return Positions.IsEmpty(); }

		/** Get source at index */
		const FVertexSource& GetSource(int32 Index) const { return Sources[Index]; }

		/** Check if vertex at index has valid source */
		bool HasValidSource(int32 Index) const { return Sources[Index].IsValid(); }

		/** Get path ID for vertex at index */
		int32 GetPathId(int32 Index) const { return Sources[Index].PathId; }

		/** Get point index for vertex at index */
		int32 GetPointIndex(int32 Index) const { return Sources[Index].PointIndex; }
	};


	/**
	 * Utility functions for contour operations
	 */
	class PCGEXELEMENTSCAVALIERCONTOURS_API FContourUtils
	{
	public:
		/**
		 * Create a polyline from input points, handling corner processing.
		 * @param Points Input points (must have PathId and PointIndex set)
		 * @param bClosed Whether to create a closed polyline
		 * @return Processed polyline with corners converted to arcs
		 */
		static FPolyline CreateFromInputPoints(const TArray<FInputPoint>& Points, const bool bClosed);

		/**
		 * Create a polyline from a root path.
		 * @param RootPath The root path containing points
		 * @return Processed polyline with corners converted to arcs
		 */
		static FPolyline CreateFromRootPath(const FRootPath& RootPath);

		/**
		 * Convert a 2D polyline back to 3D using source tracking.
		 * @param Polyline2D The 2D polyline to convert
		 * @param RootPaths Map of PathId -> RootPath for looking up source transforms
		 * @return 3D contour result with positions, transforms, and source info
		 */
		static FContourResult3D ConvertTo3D(const FPolyline& Polyline2D, const TMap<int32, FRootPath>& RootPaths, const bool bBlendTransforms = false);

	};
}

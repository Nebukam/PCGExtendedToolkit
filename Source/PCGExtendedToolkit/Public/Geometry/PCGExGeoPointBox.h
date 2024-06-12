// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGExEdge.h"

namespace PCGExGeo
{
	struct PCGEXTENDEDTOOLKIT_API FPointBox
	{
		FTransform Transform;
		FTransform InverseTransform;
		FBox Box;
		FBoxSphereBounds Sphere;
		FVector LocalCenter;
		int32 Index;

		explicit FPointBox(const FPCGPoint& InPoint, const int32 InIndex):
			Transform(FTransform(InPoint.Transform.GetRotation(), InPoint.Transform.GetLocation(), FVector::One())),
			InverseTransform(Transform.Inverse()),
			Box(FBox(InPoint.BoundsMin * InPoint.Transform.GetScale3D(), InPoint.BoundsMax * InPoint.Transform.GetScale3D())),
			Sphere(FBoxSphereBounds(Box)),
			LocalCenter(Box.GetCenter()),
			Index(InIndex)
		{
		}

		bool Contains(const FVector& Position) const { return Box.IsInside(InverseTransform.TransformPosition(Position)); }

		bool FindSegmentIntersections(
			const FVector& Start,
			const FVector& End,
			FVector& OutIntersectionA,
			FVector& OutIntersectionB,
			bool& bIsIntersectionAValid,
			bool& bIsIntersectionBValid) const
		{
			const FVector LocalStart = InverseTransform.TransformPosition(Start);
			const FVector LocalEnd = InverseTransform.TransformPosition(End);

			if (Box.IsInside(LocalStart))
			{
				bIsIntersectionBValid = false;

				if (Box.IsInside(LocalEnd))
				{
					// Both points are inside, no intersection
					bIsIntersectionAValid = false;
					return false;
				}

				// Simple intersection
				return true;
			}

			if (Box.IsInside(LocalEnd))
			{
				bIsIntersectionBValid = false;

				// Simple intersection

				return true;
			}

			// Complex intersection check

			// Rule out if the closest point on the edge is further than the sphere bound radius
			if (FVector::Dist(FMath::ClosestPointOnSegment(LocalCenter, LocalStart, LocalEnd), LocalCenter) > Sphere.SphereRadius)
			{
				bIsIntersectionAValid = false;
				bIsIntersectionBValid = false;
				return false;
			}

			// Go the complex route

			return false;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointBoxSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPointBox* InPoint)
		{
			return InPoint->Sphere;
		}

		FORCEINLINE static const bool AreElementsEqual(const FPointBox* A, const FPointBox* B)
		{
			return A->Index == B->Index;
		}

		FORCEINLINE static void ApplyOffset(FPointBox* InNode)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FPointBox* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	class PCGEXTENDEDTOOLKIT_API FPointBoxCloud
	{
		using PointBoxCloudOctree = TOctree2<FPointBox*, FPointBoxSemantics>;
		PointBoxCloudOctree* Octree = nullptr;
		TArray<FPointBox*> Boxes;
		FBox CloudBounds;

	public:
		explicit FPointBoxCloud(const UPCGPointData* PointData)
		{
			CloudBounds = PointData->GetBounds();
			Octree = new PointBoxCloudOctree(CloudBounds.GetCenter(), CloudBounds.GetExtent().Length());
			const TArray<FPCGPoint>& Points = PointData->GetPoints();

			Boxes.SetNumUninitialized(Points.Num());
			for (int i = 0; i < Points.Num(); i++)
			{
				FPointBox* NewPointBox = new FPointBox(Points[i], i);
				Boxes[i] = NewPointBox;
				Octree->AddElement(NewPointBox);
			}
		}

		~FPointBoxCloud()
		{
			PCGEX_DELETE(Octree)
		}

		bool LooseOverlaps(const UPCGPointData* InPointData) const
		{
			const FBox PointBounds = InPointData->GetBounds();
			if (CloudBounds.Intersect(PointBounds)) { return true; }
			return CloudBounds.IsInside(PointBounds);
		}
		
		bool Overlaps(const UPCGPointData* InPointData) const
		{
			const FBox PointBounds = InPointData->GetBounds();
			if (CloudBounds.Intersect(PointBounds)) { return true; }
			return CloudBounds.IsInside(PointBounds);
		}

		bool Overlaps(const TArrayView<PCGExGraph::FIndexedEdge>& Edges, const UPCGPointData* InPointData) const
		{
			const FBox PointBounds = InPointData->GetBounds();

			if (!CloudBounds.Intersect(PointBounds))
			{
				if (!CloudBounds.IsInside(PointBounds))
				{
					// No intersection nor encapsulation, don't bother
					return false;
				}
			}

			return false;
		}

		bool Encompass(const UPCGPointData* InPointData) const
		{
			const FBox PointBounds = InPointData->GetBounds();

			if (!CloudBounds.Intersect(PointBounds))
			{
				if (!CloudBounds.IsInside(PointBounds))
				{
					// No intersection nor encapsulation, don't bother
					return false;
				}
			}

			const TArray<FPCGPoint>& Points = InPointData->GetPoints();

			return false;
		}
	};
}

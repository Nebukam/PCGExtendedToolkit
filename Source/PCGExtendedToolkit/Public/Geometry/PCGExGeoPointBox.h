// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGExEdge.h"

namespace PCGExGeo
{
	struct PCGEXTENDEDTOOLKIT_API FCut
	{
		FVector Position = FVector::ZeroVector;
		int32 BoxIndex = -1;
		bool bIsInside = false;

		FCut(const FVector& InPosition, const int32 InBoxIndex):
			Position(InPosition), BoxIndex(InBoxIndex)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FIntersections
	{
		TArray<FCut> Cuts;
		FVector StartPosition = FVector::ZeroVector;
		FVector EndPosition = FVector::ZeroVector;
		int32 Start = -1;
		int32 End = -1;

		FIntersections(const FVector& InStartPosition, const FVector& InEndPosition, const int32 InStart, const int32 InEnd):
			StartPosition(InStartPosition), EndPosition(InEndPosition), Start(InStart), End(InEnd)
		{
		}

		FIntersections(const int32 InStart, const int32 InEnd):
			Start(InStart), End(InEnd)
		{
		}

		FORCEINLINE uint64 GetKey() const { return PCGEx::H64U(Start, End); }

		void Sort()
		{
			Cuts.Sort(
				[&](const FCut& A, const FCut& B)
				{
					return FVector::DistSquared(StartPosition, B.Position) < FVector::DistSquared(StartPosition, B.Position);
				});
		}

		void SortAndDedupe()
		{
			Sort();

			if (Cuts.IsEmpty() || Cuts.Num() < 2) { return; }

			FVector LastPos = Cuts[0].Position;

			for (int i = 1; i < Cuts.Num(); i++)
			{
				FVector Pos = (Cuts.GetData() + i)->Position;
				if (Pos == LastPos)
				{
					Cuts.RemoveAt(i);
					i--;
				}
				LastPos = Pos;
			}
		}

		FBoxCenterAndExtent GetBoxCenterAndExtent() const
		{
			FBox Box = FBox(ForceInit);
			Box += StartPosition;
			Box += EndPosition;
			return FBoxCenterAndExtent(Box);
		}

		void Insert(const FVector& Position, const int32 Index)
		{
			Cuts.Emplace(Position, Index);
		}
	};

	class PCGEXTENDEDTOOLKIT_API FSegmentation
	{
		mutable FRWLock IntersectionsLock;

	public:
		TMap<uint64, FIntersections*> IntersectionsMap;
		TArray<FIntersections*> IntersectionsList;

		FSegmentation()
		{
		}

		~FSegmentation()
		{
			PCGEX_DELETE_TMAP(IntersectionsMap, uint64)
			PCGEX_DELETE_TARRAY(IntersectionsList)
		}

		int32 GetNum() const { return IntersectionsMap.Num(); }

		int32 GetNumCuts() const
		{
			TArray<uint64> Keys;
			IntersectionsMap.GetKeys(Keys);
			int32 Sum = 0;
			for (const uint64 Key : Keys) { Sum += IntersectionsMap[Key]->Cuts.Num(); }
			Keys.Empty();
			return Sum;
		}

		void ReduceToArray()
		{
			TArray<uint64> Keys;
			IntersectionsMap.GetKeys(Keys);

			PCGEX_SET_NUM_UNINITIALIZED(IntersectionsList, Keys.Num())

			int32 Index = 0;
			for (const uint64 Key : Keys) { IntersectionsList[Index++] = IntersectionsMap[Key]; }
			Keys.Empty();
			IntersectionsMap.Empty();
		}

		FORCEINLINE FIntersections* Find(const uint64 Key)
		{
			{
				FReadScopeLock ReadScopeLock(IntersectionsLock);
				if (FIntersections** ExistingIntersection = IntersectionsMap.Find(Key)) { return *ExistingIntersection; }
			}

			return nullptr;
		}

		void Insert(FIntersections* InIntersections)
		{
			FWriteScopeLock WriteScopeLock(IntersectionsLock);
			IntersectionsMap.Add(InIntersections->GetKey(), InIntersections);
		}

		FIntersections* GetOrCreate(const int32 Start, const int32 End)
		{
			const uint64 HID = PCGEx::H64U(Start, End);
			if (FIntersections* ExistingIntersection = Find(HID)) { return ExistingIntersection; }

			FIntersections* NewIntersections = new FIntersections(Start, End);
			Insert(NewIntersections);

			return NewIntersections;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointBox
	{
		FTransform Transform;
		FBoxSphereBounds Sphere;
		FBox Box;
		int32 Index;

		explicit FPointBox(const FPCGPoint& InPoint, const int32 InIndex):
			Transform(FTransform(InPoint.Transform.GetRotation(), InPoint.Transform.GetLocation(), FVector::One())),
			Index(InIndex)
		{
			const FBox PointBox = InPoint.GetLocalBounds();
			const FVector Extents = PointBox.GetExtent() * InPoint.Transform.GetScale3D();
			const double Len = Extents.Length();

			Box = FBox(Extents * -1, Extents);
			Sphere = FBoxSphereBounds(InPoint.Transform.GetLocation(), FVector(Len), Len);
		}

		bool Contains(const FVector& Position) const { return Box.IsInside(Transform.InverseTransformPosition(Position)); }

		bool Intersect(const FPCGPoint& Point, EPCGExPointBoundsSource BoundsSource) const
		{
			const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.Transform).InverseTransformBy(Transform);
			return Box.Intersect(LocalBox);
		}

		bool Contains(const FPCGPoint& Point, EPCGExPointBoundsSource BoundsSource) const
		{
			const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.Transform).InverseTransformBy(Transform);
			return Box.IsInside(LocalBox);
		}

		bool ContainsOrIntersect(const FPCGPoint& Point, EPCGExPointBoundsSource BoundsSource, bool& bContains, bool& bIntersects) const
		{
			const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.Transform).InverseTransformBy(Transform);
			bContains = Box.IsInside(LocalBox);
			bIntersects = Box.Intersect(LocalBox);
			return bContains || bIntersects;
		}

		bool ProcessIntersections(FIntersections* InIntersections) const
		{
			FVector OutIntersection1 = FVector::ZeroVector;
			FVector OutIntersection2 = FVector::ZeroVector;
			bool bIsIntersection2Valid = false;
			if (SegmentIntersection(InIntersections->StartPosition, InIntersections->EndPosition, OutIntersection1, OutIntersection2, bIsIntersection2Valid))
			{
				InIntersections->Insert(OutIntersection1, Index);
				if (bIsIntersection2Valid) { InIntersections->Insert(OutIntersection2, Index); }
				return true;
			}
			return false;
		}

		bool SegmentIntersection(
			const FVector& Start,
			const FVector& End,
			FVector& OutI1,
			FVector& OutI2,
			bool& bIsI2Valid) const
		{
			const FVector LocalStart = Transform.InverseTransformPosition(Start);
			const FVector LocalEnd = Transform.InverseTransformPosition(End);

			const bool bIsStartInside = Box.IsInside(LocalStart);
			const bool bIsEndInside = Box.IsInside(LocalEnd);

			bIsI2Valid = false;

			if (bIsStartInside && bIsEndInside) { return false; }

			FVector HitLocation;
			FVector HitNormal;
			float HitTime;

			bool bHasValidIntersection = false;

			if (bIsEndInside)
			{
				if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
				{
					OutI1 = Transform.TransformPosition(HitLocation);
					return OutI1 != Start && OutI1 != End;
				}

				return false;
			}

			if (bIsStartInside)
			{
				if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
				{
					OutI1 = Transform.TransformPosition(HitLocation);
					return OutI1 != Start && OutI1 != End;
				}

				return false;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				OutI1 = Transform.TransformPosition(HitLocation);
				bHasValidIntersection = OutI1 != Start && OutI1 != End;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				if (!bHasValidIntersection)
				{
					OutI1 = Transform.TransformPosition(HitLocation);
					bHasValidIntersection = OutI1 != Start && OutI1 != End;
				}
				else
				{
					OutI2 = Transform.TransformPosition(HitLocation);
					bIsI2Valid = OutI1 != OutI2 && (OutI2 != Start && OutI2 != End);
				}

				bHasValidIntersection = bHasValidIntersection || bIsI2Valid;
			}

			return bHasValidIntersection;
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
			Octree = new PointBoxCloudOctree(CloudBounds.GetCenter(), CloudBounds.GetExtent().Length() * 1.5);
			const TArray<FPCGPoint>& Points = PointData->GetPoints();

			PCGEX_SET_NUM_UNINITIALIZED(Boxes, Points.Num())

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

		bool FindIntersections(FIntersections* InIntersections) const
		{
			const FBoxCenterAndExtent BCAE = InIntersections->GetBoxCenterAndExtent();
			Octree->FindElementsWithBoundsTest(BCAE, [&](const FPointBox* NearbyBox) { NearbyBox->ProcessIntersections(InIntersections); });
			return !InIntersections->Cuts.IsEmpty();
		}

		bool Overlaps(const FVector& InPosition) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			bool bOverlapFound = false;
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox) { if (NearbyBox->Contains(InPosition)) { bOverlapFound = true; } });
			return bOverlapFound;
		}

		bool Overlaps(const FVector& InPosition, TArray<FPointBox*>& OutOverlaps) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (NearbyBox->Contains(InPosition))
					{
						OutOverlaps.Add(*(Boxes.GetData() + NearbyBox->Index));
					}
				});
			return !OutOverlaps.IsEmpty();
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

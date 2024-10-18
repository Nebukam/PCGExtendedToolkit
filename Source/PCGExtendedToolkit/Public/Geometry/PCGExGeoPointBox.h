// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGPointData.h"

namespace PCGExGeo
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCut
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		int32 BoxIndex = -1;
		bool bIsInside = false;

		FCut(const FVector& InPosition, const FVector& InNormal, const int32 InBoxIndex):
			Position(InPosition), Normal(InNormal), BoxIndex(InBoxIndex)
		{
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSample
	{
		FVector Distances = FVector::ZeroVector;
		FVector UVW = FVector::ZeroVector;
		double Weight = 0;
		int32 BoxIndex = -1;
		bool bIsInside = false;

		FSample()
		{
		}

		FSample(const FVector& InDistances, const int32 InBoxIndex, const bool IsInside):
			Distances(InDistances), BoxIndex(InBoxIndex), bIsInside(IsInside)
		{
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FIntersections
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

		void Insert(const FVector& Position, const FVector& Normal, const int32 Index)
		{
			Cuts.Emplace(Position, Normal, Index);
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FSegmentation
	{
		mutable FRWLock IntersectionsLock;

	public:
		TMap<uint64, TSharedPtr<FIntersections>> IntersectionsMap;
		TArray<TSharedPtr<FIntersections>> IntersectionsList;

		FSegmentation()
		{
		}

		~FSegmentation() = default;

		int32 GetNum() const { return IntersectionsMap.Num(); }

		int32 GetNumCuts() const
		{
			int32 Sum = 0;
			for (const TPair<uint64, TSharedPtr<FIntersections>>& Pair : IntersectionsMap) { Sum += Pair.Value->Cuts.Num(); }
			return Sum;
		}

		void ReduceToArray()
		{
			PCGEx::InitArray(IntersectionsList, IntersectionsMap.Num());
			int32 Index = 0;
			for (const TPair<uint64, TSharedPtr<FIntersections>>& Pair : IntersectionsMap) { IntersectionsList[Index++] = Pair.Value; }
			IntersectionsMap.Empty();
		}

		FORCEINLINE TSharedPtr<FIntersections> Find(const uint64 Key)
		{
			{
				FReadScopeLock ReadScopeLock(IntersectionsLock);
				if (TSharedPtr<FIntersections>* ExistingIntersection = IntersectionsMap.Find(Key)) { return *ExistingIntersection; }
			}

			return nullptr;
		}

		void Insert(const TSharedPtr<FIntersections>& InIntersections)
		{
			FWriteScopeLock WriteScopeLock(IntersectionsLock);
			IntersectionsMap.Add(InIntersections->GetKey(), InIntersections);
		}

		TSharedPtr<FIntersections> GetOrCreate(const int32 Start, const int32 End)
		{
			const uint64 HID = PCGEx::H64U(Start, End);
			if (TSharedPtr<FIntersections> ExistingIntersection = Find(HID)) { return ExistingIntersection; }

			TSharedPtr<FIntersections> NewIntersections = MakeShared<FIntersections>(Start, End);
			Insert(NewIntersections);

			return NewIntersections;
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBox
	{
		FTransform Transform;
		FMatrix InvMatrix;
		FBoxSphereBounds Sphere;
		FVector Extents;
		FBox Box;
		FBox EpsilonBox;
		double Len;
		int32 Index;

		explicit FPointBox(const FPCGPoint& InPoint, const int32 InIndex, const EPCGExPointBoundsSource BoundsSource, double Epsilon = DBL_EPSILON):
			Transform(FTransform(InPoint.Transform.GetRotation(), InPoint.Transform.GetLocation(), FVector::One())),
			InvMatrix(Transform.Inverse().ToMatrixNoScale()),
			Index(InIndex)
		{
			const FBox PointBox = PCGExMath::GetLocalBounds(InPoint, BoundsSource);
			Extents = PointBox.GetExtent();
			Len = Extents.Length();

			Box = FBox(Extents * -1, Extents);

			EpsilonBox = Box.ExpandBy(-Epsilon);
			Sphere = FBoxSphereBounds(Transform.GetLocation(), FVector(Len), Len);
		}

#pragma region Position checks

		FORCEINLINE bool IsInside(const FVector& Position) const { return Box.IsInside(Transform.InverseTransformPosition(Position)); }
		FORCEINLINE bool IsInsideMinusEpsilon(const FVector& Position) const { return EpsilonBox.IsInside(Transform.InverseTransformPosition(Position)); }

#pragma endregion

#pragma region Point checks

#define PCGEX_TRANSFORM_LOCALBOUNDS const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.Transform.ToMatrixNoScale() * InvMatrix);
#define PCGEX_TRANSFORM_LOCALBOUNDS_T const FBox LocalBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale() * InvMatrix);

		FORCEINLINE bool Intersect(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			return Box.Intersect(LocalBox);
		}

		FORCEINLINE bool IsInside(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			return Box.IsInside(LocalBox);
		}

		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
#if PCGEX_ENGINE_VERSION <= 503
			return Box.IsInside(LocalBox);
#else
			return Box.IsInsideOrOn(LocalBox);
#endif
		}

		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
#if PCGEX_ENGINE_VERSION <= 503
			return Box.IsInside(LocalBox) || Box.Intersect(LocalBox);
#else
			return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox);
#endif
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool Intersect(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			return Box.Intersect(LocalBox);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInside(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			return Box.IsInside(LocalBox);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
#if PCGEX_ENGINE_VERSION <= 503
			return Box.IsInside(LocalBox);
#else
			return Box.IsInsideOrOn(LocalBox);
#endif
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
#if PCGEX_ENGINE_VERSION <= 503
			return Box.IsInside(LocalBox) || Box.Intersect(LocalBox);
#else
			return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox);
#endif
		}

#undef PCGEX_TRANSFORM_LOCALBOUNDS
#undef PCGEX_TRANSFORM_LOCALBOUNDS_T

#pragma endregion

		FORCEINLINE void Sample(const FVector& Position, FSample& OutSample) const
		{
			const FVector LocalPosition = Transform.InverseTransformPosition(Position);
			OutSample.bIsInside = Box.IsInside(LocalPosition);
			OutSample.Distances = LocalPosition;
			OutSample.BoxIndex = Index;
			OutSample.UVW = FVector(
				LocalPosition.X / Extents.X,
				LocalPosition.Y / Extents.Y,
				LocalPosition.Z / Extents.Z);
			OutSample.Weight = 1 - ((
				(FMath::Clamp(FMath::Abs(OutSample.UVW.X), 0, Extents.X) / Extents.X) +
				(FMath::Clamp(FMath::Abs(OutSample.UVW.Y), 0, Extents.Y) / Extents.Y) +
				(FMath::Clamp(FMath::Abs(OutSample.UVW.Z), 0, Extents.Z) / Extents.Z)) / 3);
		}

		FORCEINLINE void Sample(const FPCGPoint& Point, FSample& OutSample) const
		{
			Sample(Point.Transform.GetLocation(), OutSample);
		}

#pragma region Intersections

		bool ProcessIntersections(FIntersections* InIntersections) const
		{
			FVector OutIntersection1 = FVector::ZeroVector;
			FVector OutIntersection2 = FVector::ZeroVector;
			FVector OutHitNormal1 = FVector::ZeroVector;
			FVector OutHitNormal2 = FVector::ZeroVector;
			bool bIsIntersection2Valid = false;
			bool bInverseDir = false;
			if (SegmentIntersection(
				InIntersections->StartPosition, InIntersections->EndPosition,
				OutIntersection1, OutIntersection2, bIsIntersection2Valid,
				OutHitNormal1, OutHitNormal2, bInverseDir))
			{
				InIntersections->Insert(OutIntersection1, OutHitNormal1, Index);
				if (bIsIntersection2Valid) { InIntersections->Insert(OutIntersection2, OutHitNormal2, Index); }
				return true;
			}
			return false;
		}

		bool SegmentIntersection(
			const FVector& Start,
			const FVector& End,
			FVector& OutIntersection1,
			FVector& OutIntersection2,
			bool& bIsI2Valid,
			FVector& OutHitNormal1,
			FVector& OutHitNormal2,
			bool& bInverseDir) const
		{
			const FVector LocalStart = Transform.InverseTransformPosition(Start);
			const FVector LocalEnd = Transform.InverseTransformPosition(End);

			const bool bIsStartInside = Box.IsInside(LocalStart);
			const bool bIsEndInside = Box.IsInside(LocalEnd);

			bIsI2Valid = false;
			bInverseDir = false;

			if (bIsStartInside && bIsEndInside) { return false; }

			FVector HitLocation;
			FVector HitNormal;
			float HitTime;

			bool bHasValidIntersection = false;

			if (bIsEndInside)
			{
				if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
				{
					OutIntersection1 = Transform.TransformPosition(HitLocation);
					OutHitNormal1 = Transform.TransformVector(HitNormal);
					return OutIntersection1 != Start && OutIntersection1 != End;
				}

				return false;
			}

			if (bIsStartInside)
			{
				if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
				{
					OutIntersection1 = Transform.TransformPosition(HitLocation);
					OutHitNormal1 = Transform.TransformVector(HitNormal);
					bInverseDir = true;
					return OutIntersection1 != Start && OutIntersection1 != End;
				}

				return false;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				OutIntersection1 = Transform.TransformPosition(HitLocation);
				OutHitNormal1 = Transform.TransformVector(HitNormal);
				bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				if (!bHasValidIntersection)
				{
					OutIntersection1 = Transform.TransformPosition(HitLocation);
					OutHitNormal1 = Transform.TransformVector(HitNormal);
					bInverseDir = true;
					bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
				}
				else
				{
					OutIntersection2 = Transform.TransformPosition(HitLocation);
					OutHitNormal2 = Transform.TransformVector(HitNormal);
					bIsI2Valid = OutIntersection1 != OutIntersection2 && (OutIntersection2 != Start && OutIntersection2 != End);
				}

				bHasValidIntersection = bHasValidIntersection || bIsI2Valid;
			}

			return bHasValidIntersection;
		}

#pragma endregion
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBoxSemantics
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

	class /*PCGEXTENDEDTOOLKIT_API*/ FPointBoxCloud
	{
		using PointBoxCloudOctree = TOctree2<FPointBox*, FPointBoxSemantics>;
		TUniquePtr<PointBoxCloudOctree> Octree;
		TArray<TSharedPtr<FPointBox>> Boxes;
		FBox CloudBounds;

	public:
		explicit FPointBoxCloud(const UPCGPointData* PointData, const EPCGExPointBoundsSource BoundsSource, const double Epsilon = DBL_EPSILON)
		{
			CloudBounds = PointData->GetBounds();
			Octree = MakeUnique<PointBoxCloudOctree>(CloudBounds.GetCenter(), CloudBounds.GetExtent().Length() * 1.5);
			const TArray<FPCGPoint>& Points = PointData->GetPoints();

			CloudBounds = FBox(ForceInit);

			Boxes.Init(nullptr, Points.Num());

			for (int i = 0; i < Points.Num(); i++)
			{
				TSharedPtr<FPointBox> NewPointBox = MakeShared<FPointBox>(Points[i], i, BoundsSource, Epsilon);
				CloudBounds += NewPointBox->Box.TransformBy(NewPointBox->Transform);
				Boxes[i] = NewPointBox;
				Octree->AddElement(NewPointBox.Get());
			}
		}

		FORCEINLINE const PointBoxCloudOctree* GetOctree() const { return Octree.Get(); }

		~FPointBoxCloud()
		{
		}

		bool FindIntersections(FIntersections* InIntersections) const
		{
			const FBoxCenterAndExtent BCAE = InIntersections->GetBoxCenterAndExtent();
			Octree->FindElementsWithBoundsTest(BCAE, [&](const FPointBox* NearbyBox) { NearbyBox->ProcessIntersections(InIntersections); });
			return !InIntersections->Cuts.IsEmpty();
		}

#pragma region Position checks

		bool IsInside(const FVector& InPosition) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			bool bOverlapFound = false;
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (!bOverlapFound && NearbyBox->IsInside(InPosition)) { bOverlapFound = true; }
				});
			return bOverlapFound;
		}

		bool IsInsideMinusEpsilon(const FVector& InPosition) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			bool bOverlapFound = false;
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (!bOverlapFound && NearbyBox->IsInsideMinusEpsilon(InPosition)) { bOverlapFound = true; }
				});
			return bOverlapFound;
		}

		bool IsInside(const FVector& InPosition, TArray<TSharedPtr<FPointBox>>& OutOverlaps) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (NearbyBox->IsInside(InPosition)) { OutOverlaps.Add(*(Boxes.GetData() + NearbyBox->Index)); }
				});
			return !OutOverlaps.IsEmpty();
		}

		bool IsInsideMinusEpsilon(const FVector& InPosition, TArray<TSharedPtr<FPointBox>>& OutOverlaps) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (NearbyBox->IsInsideMinusEpsilon(InPosition)) { OutOverlaps.Add(*(Boxes.GetData() + NearbyBox->Index)); }
				});
			return !OutOverlaps.IsEmpty();
		}

#pragma endregion

#pragma region Point checks

#define PCGEX_POINT_BOUNDS_CHECK(_NAME) \
		bool bIntersectFound = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
				if (NearbyBox->_NAME(Point, BoundsSource)){ bIntersectFound = true; return false;} return true; }); return bIntersectFound;
#define PCGEX_POINT_BOUNDS_CHECK_T(_NAME) \
		bool bIntersectFound = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
		if (NearbyBox->_NAME<S>(Point)){ bIntersectFound = true; return false;} return true; }); return bIntersectFound;
		FORCEINLINE bool Intersect(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(Intersect)
		}

		FORCEINLINE bool IsInside(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInside)
		}

		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrOn)
		}

		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrIntersects)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool Intersect(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(Intersect)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInside(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInside)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrOn)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrIntersects)
		}

#undef PCGEX_POINT_BOUNDS_CHECK

#pragma endregion

#pragma region Point data checks

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

#pragma endregion

		bool Sample(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource, TArray<FSample>& OutSample) const
		{
			const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Point.Transform.GetLocation(), PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());
			Octree->FindElementsWithBoundsTest(
				BCAE, [&](const FPointBox* NearbyBox)
				{
					FSample& Sample = OutSample.Emplace_GetRef();
					NearbyBox->Sample(Point, Sample);
				});

			return !OutSample.IsEmpty();
		}
	};
}

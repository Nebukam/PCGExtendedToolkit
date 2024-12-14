// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGPointData.h"

#include "PCGExGeoPointBox.generated.h"

UENUM()
enum class EPCGExBoxCheckMode : uint8
{
	Box            = 0 UMETA(DisplayName = "Box", Tooltip="A box"),
	ExpandedBox    = 1 UMETA(DisplayName = "Expanded Box", Tooltip="A box expanded by an amount"),
	Sphere         = 2 UMETA(DisplayName = "Sphere", Tooltip="Sphere which radius is defined by the bounds' extents size"),
	ExpandedSphere = 3 UMETA(DisplayName = "Expanded Sphere", Tooltip="A box expanded by an amount"),
};

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

			PCGEX_MAKE_SHARED(NewIntersections, FIntersections, Start, End)
			Insert(NewIntersections);

			return NewIntersections;
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBox
	{
		FMatrix Matrix;
		FMatrix InvMatrix;
		FBoxSphereBounds SearchableBounds;
		FBox Box;
		FBox BoxExpanded;
		FVector Extents;
		double RadiusSquared;
		double RadiusSquaredExpanded;
		int32 Index;

		explicit FPointBox(const FPCGPoint& InPoint, const int32 InIndex, const EPCGExPointBoundsSource BoundsSource, double Expansion = DBL_EPSILON):
			Matrix(InPoint.Transform.ToMatrixNoScale()),
			InvMatrix(Matrix.Inverse()),
			Index(InIndex)
		{
			const FBox PointBox = PCGExMath::GetLocalBounds(InPoint, BoundsSource);
			Extents = PointBox.GetExtent();
			double Size = Extents.Length();
			double SanitizedExpansion = Expansion < 0 ? FMath::Max(Expansion, -Size) : Expansion;

			Box = FBox(Extents * -1, Extents);
			BoxExpanded = Box.ExpandBy(Expansion);
			SearchableBounds = FBoxSphereBounds(InPoint.Transform.GetLocation(), FVector(Size + SanitizedExpansion * 1.5), Size + SanitizedExpansion * 1.5);
			RadiusSquared = FMath::Square(Size);
			RadiusSquaredExpanded = FMath::Square(Size + SanitizedExpansion);
		}

#pragma region Position checks

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInside(const FVector& Position) const
		{
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return BoxExpanded.IsInside(Matrix.InverseTransformPosition(Position)); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return Box.IsInside(Matrix.InverseTransformPosition(Position)); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return (SearchableBounds.Origin - Position).SizeSquared() <= RadiusSquared; }
			else { return (SearchableBounds.Origin - Position).SizeSquared() <= RadiusSquaredExpanded; }
		}

#pragma endregion

#pragma region Point checks

#define PCGEX_TRANSFORM_LOCALBOUNDS const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.Transform.ToMatrixNoScale() * InvMatrix);
#define PCGEX_TRANSFORM_LOCALBOUNDS_T const FBox LocalBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale() * InvMatrix);

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool Intersect(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox); }
			else { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox); }
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInside(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					LocalBox.Min.SizeSquared() >= RadiusSquared ||
					LocalBox.Max.SizeSquared() >= RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					LocalBox.Min.SizeSquared() >= RadiusSquaredExpanded ||
					LocalBox.Max.SizeSquared() >= RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
#if PCGEX_ENGINE_VERSION <= 503
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
#else
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox); }
#endif
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					LocalBox.Min.SizeSquared() > RadiusSquared ||
					LocalBox.Max.SizeSquared() > RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					LocalBox.Min.SizeSquared() > RadiusSquaredExpanded ||
					LocalBox.Max.SizeSquared() > RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
#if PCGEX_ENGINE_VERSION <= 503
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox) || Box.Intersect(LocalBox);; }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox) || BoxExpanded.Intersect(LocalBox); }
#else
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox) || BoxExpanded.Intersect(LocalBox); }
#endif
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					(LocalBox.Min.SizeSquared() <= RadiusSquared && LocalBox.Max.SizeSquared() <= RadiusSquared))
				{
					return true;
				}
				return false;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					(LocalBox.Min.SizeSquared() <= RadiusSquaredExpanded && LocalBox.Max.SizeSquared() <= RadiusSquaredExpanded))
				{
					return true;
				}
				return false;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool Intersect(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox); }
			else { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox); }
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInside(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					LocalBox.Min.SizeSquared() >= RadiusSquared ||
					LocalBox.Max.SizeSquared() >= RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					LocalBox.Min.SizeSquared() >= RadiusSquaredExpanded ||
					LocalBox.Max.SizeSquared() >= RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
#if PCGEX_ENGINE_VERSION <= 503
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
#else
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox); }
#endif
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					LocalBox.Min.SizeSquared() > RadiusSquared ||
					LocalBox.Max.SizeSquared() > RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					LocalBox.Min.SizeSquared() > RadiusSquaredExpanded ||
					LocalBox.Max.SizeSquared() > RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
#if PCGEX_ENGINE_VERSION <= 503
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox) || Box.Intersect(LocalBox);; }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox) || BoxExpanded.Intersect(LocalBox); }
#else
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox) || BoxExpanded.Intersect(LocalBox); }
#endif
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) ||
					(LocalBox.Min.SizeSquared() <= RadiusSquared && LocalBox.Max.SizeSquared() <= RadiusSquared))
				{
					return true;
				}
				return false;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) ||
					(LocalBox.Min.SizeSquared() <= RadiusSquaredExpanded && LocalBox.Max.SizeSquared() <= RadiusSquaredExpanded))
				{
					return true;
				}
				return false;
			}
		}

#undef PCGEX_TRANSFORM_LOCALBOUNDS
#undef PCGEX_TRANSFORM_LOCALBOUNDS_T

#pragma endregion

		FORCEINLINE void Sample(const FVector& Position, FSample& OutSample) const
		{
			const FVector LocalPosition = Matrix.InverseTransformPosition(Position);
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
			const FVector LocalStart = Matrix.InverseTransformPosition(Start);
			const FVector LocalEnd = Matrix.InverseTransformPosition(End);

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
					OutIntersection1 = Matrix.TransformPosition(HitLocation);
					OutHitNormal1 = Matrix.TransformVector(HitNormal);
					return OutIntersection1 != Start && OutIntersection1 != End;
				}

				return false;
			}

			if (bIsStartInside)
			{
				if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
				{
					OutIntersection1 = Matrix.TransformPosition(HitLocation);
					OutHitNormal1 = Matrix.TransformVector(HitNormal);
					bInverseDir = true;
					return OutIntersection1 != Start && OutIntersection1 != End;
				}

				return false;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				OutIntersection1 = Matrix.TransformPosition(HitLocation);
				OutHitNormal1 = Matrix.TransformVector(HitNormal);
				bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
			}

			if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				if (!bHasValidIntersection)
				{
					OutIntersection1 = Matrix.TransformPosition(HitLocation);
					OutHitNormal1 = Matrix.TransformVector(HitNormal);
					bInverseDir = true;
					bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
				}
				else
				{
					OutIntersection2 = Matrix.TransformPosition(HitLocation);
					OutHitNormal2 = Matrix.TransformVector(HitNormal);
					bIsI2Valid = OutIntersection1 != OutIntersection2 && (OutIntersection2 != Start && OutIntersection2 != End);
				}

				bHasValidIntersection = bHasValidIntersection || bIsI2Valid;
			}

			return bHasValidIntersection;
		}

#pragma endregion
	};

	PCGEX_OCTREE_SEMANTICS(FPointBox, { return Element->SearchableBounds;}, { return A->Index == B->Index; })

	class /*PCGEXTENDEDTOOLKIT_API*/ FPointBoxCloud : public TSharedFromThis<FPointBoxCloud>
	{
		TUniquePtr<FPointBoxOctree> Octree;
		TArray<TSharedPtr<FPointBox>> Boxes;
		FBox CloudBounds;

		FVector SearchPadding;

	public:
		explicit FPointBoxCloud(const UPCGPointData* PointData, const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON)
		{
			CloudBounds = PointData->GetBounds();
			Octree = MakeUnique<FPointBoxOctree>(CloudBounds.GetCenter(), CloudBounds.GetExtent().Length() * 1.5);
			const TArray<FPCGPoint>& Points = PointData->GetPoints();

			SearchPadding = FVector(FMath::Abs(Expansion) * 2);
			CloudBounds = FBox(ForceInit);

			Boxes.Init(nullptr, Points.Num());

			for (int i = 0; i < Points.Num(); i++)
			{
				PCGEX_MAKE_SHARED(NewPointBox, FPointBox, Points[i], i, BoundsSource, Expansion)
				CloudBounds += NewPointBox->Box.TransformBy(NewPointBox->Matrix);
				Boxes[i] = NewPointBox;
				Octree->AddElement(NewPointBox.Get());
			}
		}

		FORCEINLINE const FPointBoxOctree* GetOctree() const { return Octree.Get(); }

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

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		bool IsInside(const FVector& InPosition) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			bool bOverlapFound = false;
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (!bOverlapFound && NearbyBox->IsInside<Mode>(InPosition)) { bOverlapFound = true; }
				});
			return bOverlapFound;
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		bool IsInside(const FVector& InPosition, TArray<TSharedPtr<FPointBox>>& OutOverlaps) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			Octree->FindNearbyElements(
				InPosition, [&](const FPointBox* NearbyBox)
				{
					if (NearbyBox->IsInside<Mode>(InPosition)) { OutOverlaps.Add(*(Boxes.GetData() + NearbyBox->Index)); }
				});
			return !OutOverlaps.IsEmpty();
		}

#pragma endregion

#pragma region Point checks

#define PCGEX_POINT_BOUNDS_CHECK(_NAME) \
		bool bResult = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
				if (NearbyBox->_NAME<Mode>(Point, BoundsSource)){ bResult = true; return false;} return true; }); return bResult;

#define PCGEX_POINT_BOUNDS_CHECK_T(_NAME) \
		bool bResult = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
				if (NearbyBox->_NAME<S, Mode>(Point)){ bResult = true; return false;} return true; }); return bResult;

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool Intersect(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(Intersect)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInside(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInside)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrOn)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrIntersects)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool Intersect(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(Intersect)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInside(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInside)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrOn(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrOn)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrIntersects(const FPCGPoint& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrIntersects)
		}

		//

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IntersectCloud(const FPCGPoint& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale());
			return PtBox.Intersect(CloudBounds);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideCloud(const FPCGPoint& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale());
			return PtBox.IsInside(CloudBounds);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrOnCloud(const FPCGPoint& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale());
#if PCGEX_ENGINE_VERSION <= 503
			return PtBox.IsInside(CloudBounds);
#else
			return PtBox.IsInsideOrOn(CloudBounds);
#endif
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		FORCEINLINE bool IsInsideOrIntersectsCloud(const FPCGPoint& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.Transform.ToMatrixNoScale());
#if PCGEX_ENGINE_VERSION <= 503
			return PtBox.IsInside(CloudBounds) || PtBox.Intersect(CloudBounds);
#else
			return PtBox.IsInsideOrOn(CloudBounds) || PtBox.Intersect(CloudBounds);
#endif
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

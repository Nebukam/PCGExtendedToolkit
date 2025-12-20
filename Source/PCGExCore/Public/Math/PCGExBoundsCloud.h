// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOctree.h"
#include "PCGExMathBounds.h"
#include "PCGExMath.h"

#include "PCGExBoundsCloud.generated.h"

class UPCGBasePointData;

UENUM()
enum class EPCGExBoxCheckMode : uint8
{
	Box            = 0 UMETA(DisplayName = "Box", Tooltip="A box"),
	ExpandedBox    = 1 UMETA(DisplayName = "Expanded Box", Tooltip="A box expanded by an amount"),
	Sphere         = 2 UMETA(DisplayName = "Sphere", Tooltip="Sphere which radius is defined by the bounds' extents size"),
	ExpandedSphere = 3 UMETA(DisplayName = "Expanded Sphere", Tooltip="A Sphere which radius is defined by the bound' extents size, expanded by an amount"),
};

UENUM()
enum class EPCGExCutType : uint8
{
	Undefined   = 0 UMETA(DisplayName = "Undefined"),
	Entry       = 1 UMETA(DisplayName = "Entry"),
	EntryNoExit = 2 UMETA(DisplayName = "Entry (no exit)"),
	Exit        = 3 UMETA(DisplayName = "Exit"),
	ExitNoEntry = 4 UMETA(DisplayName = "Exit (no entry)"),
};

namespace PCGExMath
{
	struct PCGEXCORE_API FCut
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		int32 BoxIndex = -1;
		int32 Idx = -1;
		EPCGExCutType Type = EPCGExCutType::Undefined;

		FCut() = default;
		FCut(const FVector& InPosition, const FVector& InNormal, const int32 InBoxIndex, const int32 InIdx, const EPCGExCutType InType = EPCGExCutType::Undefined);
	};

	struct PCGEXCORE_API FSample
	{
		FVector Distances = FVector::ZeroVector;
		FVector UVW = FVector::ZeroVector;
		double Weight = 0;
		int32 BoxIndex = -1;
		bool bIsInside = false;

		FSample() = default;
		FSample(const FVector& InDistances, const int32 InBoxIndex, const bool IsInside);
	};

	struct PCGEXCORE_API FIntersections
	{
		TArray<FCut> Cuts;
		FVector StartPosition = FVector::ZeroVector;
		FVector EndPosition = FVector::ZeroVector;

		FIntersections(const FVector& InStartPosition, const FVector& InEndPosition);

		bool IsEmpty() const;

		void Sort();
		void SortAndDedupe();

		FBoxCenterAndExtent GetBoxCenterAndExtent() const;

		void Insert(const FVector& Position, const FVector& Normal, const int32 Index, const int32 Idx = -1, const EPCGExCutType Type = EPCGExCutType::Undefined);
	};

	struct PCGEXCORE_API FPointBox
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

		explicit FPointBox(const PCGExData::FConstPoint& InPoint, const int32 InIndex, const EPCGExPointBoundsSource BoundsSource, double Expansion = DBL_EPSILON);

#pragma region Position checks

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		bool IsInside(const FVector& Position) const
		{
			if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(Matrix.InverseTransformPosition(Position)); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(Matrix.InverseTransformPosition(Position)); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return (SearchableBounds.Origin - Position).SizeSquared() <= RadiusSquared; }
			else { return (SearchableBounds.Origin - Position).SizeSquared() <= RadiusSquaredExpanded; }
		}

#pragma endregion

#pragma region Point checks

#define PCGEX_TRANSFORM_LOCALBOUNDS const FBox LocalBox = PCGExMath::GetLocalBounds(Point, BoundsSource).TransformBy(Point.GetTransform().ToMatrixNoScale() * InvMatrix);
#define PCGEX_TRANSFORM_LOCALBOUNDS_T const FBox LocalBox = PCGExMath::GetLocalBounds<S, PointType>(Point).TransformBy(Point.GetTransform().ToMatrixNoScale() * InvMatrix);

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool Intersect(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox); }
			else { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox); }
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInside(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || LocalBox.Min.SizeSquared() >= RadiusSquared || LocalBox.Max.SizeSquared() >= RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || LocalBox.Min.SizeSquared() >= RadiusSquaredExpanded || LocalBox.Max.SizeSquared() >= RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrOn(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS

			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || LocalBox.Min.SizeSquared() > RadiusSquared || LocalBox.Max.SizeSquared() > RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || LocalBox.Min.SizeSquared() > RadiusSquaredExpanded || LocalBox.Max.SizeSquared() > RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrIntersects(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox) || BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || (LocalBox.Min.SizeSquared() <= RadiusSquared && LocalBox.Max.SizeSquared() <= RadiusSquared))
				{
					return true;
				}
				return false;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || (LocalBox.Min.SizeSquared() <= RadiusSquaredExpanded && LocalBox.Max.SizeSquared() <= RadiusSquaredExpanded))
				{
					return true;
				}
				return false;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool Intersect(const PointType& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere) { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox); }
			else { return FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox); }
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInside(const PointType& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInside(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || LocalBox.Min.SizeSquared() >= RadiusSquared || LocalBox.Max.SizeSquared() >= RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || LocalBox.Min.SizeSquared() >= RadiusSquaredExpanded || LocalBox.Max.SizeSquared() >= RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrOn(const PointType& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || LocalBox.Min.SizeSquared() > RadiusSquared || LocalBox.Max.SizeSquared() > RadiusSquared)
				{
					return false;
				}
				return true;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || LocalBox.Min.SizeSquared() > RadiusSquaredExpanded || LocalBox.Max.SizeSquared() > RadiusSquaredExpanded)
				{
					return false;
				}
				return true;
			}
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrIntersects(const PointType& Point) const
		{
			PCGEX_TRANSFORM_LOCALBOUNDS_T
			if constexpr (Mode == EPCGExBoxCheckMode::Box) { return Box.IsInsideOrOn(LocalBox) || Box.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::ExpandedBox) { return BoxExpanded.IsInsideOrOn(LocalBox) || BoxExpanded.Intersect(LocalBox); }
			else if constexpr (Mode == EPCGExBoxCheckMode::Sphere)
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquared, LocalBox) || (LocalBox.Min.SizeSquared() <= RadiusSquared && LocalBox.Max.SizeSquared() <= RadiusSquared))
				{
					return true;
				}
				return false;
			}
			else
			{
				if (FMath::SphereAABBIntersection(FVector::ZeroVector, RadiusSquaredExpanded, LocalBox) || (LocalBox.Min.SizeSquared() <= RadiusSquaredExpanded && LocalBox.Max.SizeSquared() <= RadiusSquaredExpanded))
				{
					return true;
				}
				return false;
			}
		}

#undef PCGEX_TRANSFORM_LOCALBOUNDS
#undef PCGEX_TRANSFORM_LOCALBOUNDS_T

#pragma endregion

		void Sample(const FVector& Position, FSample& OutSample) const;
		void Sample(const PCGExData::FConstPoint& Point, FSample& OutSample) const;

#pragma region Intersections

		bool ProcessIntersections(FIntersections* InIntersections, const int32 Idx = -1) const;
		bool SegmentIntersection(const FVector& Start, const FVector& End, FVector& OutIntersection1, FVector& OutIntersection2, bool& bIsI2Valid, FVector& OutHitNormal1, FVector& OutHitNormal2, bool& bInverseDir) const;


#pragma endregion
	};

	PCGEX_OCTREE_SEMANTICS(FPointBox, { return Element->SearchableBounds;}, { return A->Index == B->Index; })

	class PCGEXCORE_API FBoundsCloud : public TSharedFromThis<FBoundsCloud>
	{
		TUniquePtr<FPointBoxOctree> Octree;
		TArray<TSharedPtr<FPointBox>> Boxes;
		FBox CloudBounds;

		FVector SearchPadding;

	public:
		int32 Idx = -1;

		explicit FBoundsCloud(const UPCGBasePointData* PointData, const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON);

		FORCEINLINE const FPointBoxOctree* GetOctree() const { return Octree.Get(); }

		~FBoundsCloud() = default;

		bool FindIntersections(FIntersections* InIntersections) const;

#pragma region Position checks

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		bool IsInside(const FVector& InPosition) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			bool bOverlapFound = false;
			Octree->FindNearbyElements(InPosition, [&](const FPointBox* NearbyBox)
			{
				if (!bOverlapFound && NearbyBox->IsInside<Mode>(InPosition)) { bOverlapFound = true; }
			});
			return bOverlapFound;
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box>
		bool IsInside(const FVector& InPosition, TArray<TSharedPtr<FPointBox>>& OutOverlaps) const
		{
			if (!CloudBounds.IsInside(InPosition)) { return false; }
			Octree->FindNearbyElements(InPosition, [&](const FPointBox* NearbyBox)
			{
				if (NearbyBox->IsInside<Mode>(InPosition)) { OutOverlaps.Add(*(Boxes.GetData() + NearbyBox->Index)); }
			});
			return !OutOverlaps.IsEmpty();
		}

#pragma endregion

#pragma region Point checks

#define PCGEX_POINT_BOUNDS_CHECK(_NAME) \
		bool bResult = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.GetTransform().GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
				if (NearbyBox->_NAME<Mode, PointType>(Point, BoundsSource)){ bResult = true; return false;} return true; }); return bResult;

#define PCGEX_POINT_BOUNDS_CHECK_T(_NAME) \
		bool bResult = false; \
		Octree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Point.GetTransform().GetLocation(), Point.GetScaledExtents()), [&](const FPointBox* NearbyBox){ \
				if (NearbyBox->_NAME<S, Mode, PointType>(Point)){ bResult = true; return false;} return true; }); return bResult;

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool Intersect(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(Intersect)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInside(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInside)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrOn(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrOn)
		}

		template <EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrIntersects(const PointType& Point, const EPCGExPointBoundsSource BoundsSource) const
		{
			PCGEX_POINT_BOUNDS_CHECK(IsInsideOrIntersects)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool Intersect(const PointType& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(Intersect)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInside(const PointType& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInside)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrOn(const PointType& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrOn)
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrIntersects(const PointType& Point) const
		{
			PCGEX_POINT_BOUNDS_CHECK_T(IsInsideOrIntersects)
		}

		//

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IntersectCloud(const PointType& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.GetTransform().ToMatrixNoScale());
			return PtBox.Intersect(CloudBounds);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideCloud(const PointType& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.GetTransform().ToMatrixNoScale());
			return PtBox.IsInside(CloudBounds);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrOnCloud(const PointType& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.GetTransform().ToMatrixNoScale());
			return PtBox.IsInsideOrOn(CloudBounds);
		}

		template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, EPCGExBoxCheckMode Mode = EPCGExBoxCheckMode::Box, typename PointType /*= PCGExData::FConstPoint*/>
		bool IsInsideOrIntersectsCloud(const PointType& Point) const
		{
			const FBox PtBox = PCGExMath::GetLocalBounds<S>(Point).TransformBy(Point.GetTransform().ToMatrixNoScale());
			return PtBox.IsInsideOrOn(CloudBounds) || PtBox.Intersect(CloudBounds);
		}

#undef PCGEX_POINT_BOUNDS_CHECK

#pragma endregion

#pragma region Point data checks

		bool LooseOverlaps(const UPCGBasePointData* InPointData) const;
		bool Overlaps(const UPCGBasePointData* InPointData) const;
		bool Encompass(const UPCGBasePointData* InPointData) const;

#pragma endregion

		bool Sample(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource BoundsSource, TArray<FSample>& OutSample) const;
	};
}

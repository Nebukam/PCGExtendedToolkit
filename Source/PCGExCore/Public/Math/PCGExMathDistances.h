// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

namespace PCGExData
{
	struct FPoint;
}

namespace PCGExMath
{
	template <EPCGExDistance Mode, typename PointType>
	static FVector GetSpatializedCenter(const PointType& FromPoint, const FVector& FromCenter, const FVector& ToCenter)
	{
		if constexpr (Mode == EPCGExDistance::None)
		{
			return FVector::OneVector;
		}
		else if constexpr (Mode == EPCGExDistance::SphereBounds)
		{
			FVector Dir = ToCenter - FromCenter;
			Dir.Normalize();

			return FromCenter + Dir * FromPoint.GetScaledExtents().Length();
		}
		else if constexpr (Mode == EPCGExDistance::BoxBounds)
		{
			const FVector LocalTargetCenter = FromPoint.GetTransform().InverseTransformPosition(ToCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(FromPoint.GetBoundsMin(), FromPoint.GetBoundsMax(), LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return FromPoint.GetTransform().TransformPosition(LocalClosestPoint);
		}
		else
		{
			return FromCenter;
		}
	}

#pragma region Distances

	class PCGEXCORE_API FDistances : public TSharedFromThis<FDistances>
	{
	public:
		virtual ~FDistances() = default;

		bool bOverlapIsZero = false;

		FDistances()
		{
		}

		explicit FDistances(const bool InOverlapIsZero)
			: bOverlapIsZero(InOverlapIsZero)
		{
		}

		virtual FVector GetSourceCenter(const PCGExData::FPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual FVector GetTargetCenter(const PCGExData::FPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual void GetCenters(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const = 0;

		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const = 0;
		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const = 0;

		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const = 0;
		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const = 0;
	};

	template <EPCGExDistance Source, EPCGExDistance Target>
	class TDistances final : public FDistances
	{
	public:
		TDistances()
		{
		}

		explicit TDistances(const bool InOverlapIsZero)
			: FDistances(InOverlapIsZero)
		{
		}

		virtual FVector GetSourceCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override;
		virtual FVector GetTargetCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override;
		virtual void GetCenters(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const override;
		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const override;

		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const override;
		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const override;

		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const override;
	};

	extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::Center>;
	extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>;
	extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>;
	extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::None>;
	extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>;
	extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>;
	extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>;
	extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::None>;
	extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>;
	extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>;
	extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>;
	extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::None>;
	extern template class TDistances<EPCGExDistance::None, EPCGExDistance::Center>;
	extern template class TDistances<EPCGExDistance::None, EPCGExDistance::SphereBounds>;
	extern template class TDistances<EPCGExDistance::None, EPCGExDistance::BoxBounds>;
	extern template class TDistances<EPCGExDistance::None, EPCGExDistance::None>;

	PCGEXCORE_API const FDistances* GetDistances(const EPCGExDistance Source = EPCGExDistance::Center, const EPCGExDistance Target = EPCGExDistance::Center, const bool bOverlapIsZero = false);

	PCGEXCORE_API const FDistances* GetNoneDistances();

#pragma endregion
}

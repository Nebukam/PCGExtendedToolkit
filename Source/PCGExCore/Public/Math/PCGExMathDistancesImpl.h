// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExMath.h"
#include "PCGExMathDistances.h"
#include "Data/PCGExPointElements.h"

namespace PCGExMath
{
	template <EPCGExDistance Source, EPCGExDistance Target, typename Derived>
	class TDistancesBase : public IDistances
	{
	public:
		using IDistances::GetDistSquared;
		using IDistances::GetDist;

		TDistancesBase() = default;

		explicit TDistancesBase(const bool InOverlapIsZero)
			: IDistances(InOverlapIsZero)
		{
		}

		FORCEINLINE static double GetDistSquaredImpl(const FVector& SourcePos, const FVector& TargetPos)
		{
			return Derived::GetDistSquaredStatic(SourcePos, TargetPos);
		}

		FORCEINLINE static double GetDistImpl(const FVector& SourcePos, const FVector& TargetPos)
		{
			return Derived::GetDistStatic(SourcePos, TargetPos);
		}

		virtual FVector GetSourceCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return GetSpatializedCenter<Source>(FromPoint, FromCenter, ToCenter);
		}

		virtual FVector GetTargetCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return GetSpatializedCenter<Target>(FromPoint, FromCenter, ToCenter);
		}

		virtual void GetCenters(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			OutSource = GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
			OutTarget = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);
		}

		virtual double GetDistSquared(const FVector& SourcePos, const FVector& TargetPos) const override
		{
			return GetDistSquaredImpl(SourcePos, TargetPos);
		}

		virtual double GetDist(const FVector& SourcePos, const FVector& TargetPos) const override
		{
			return GetDistImpl(SourcePos, TargetPos);
		}

		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const override
		{
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourcePos = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector TargetPos = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, SourcePos);
			return GetDistSquaredImpl(SourcePos, TargetPos);
		}

		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const override
		{
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourcePos = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector TargetPos = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, SourcePos);
			return GetDistImpl(SourcePos, TargetPos);
		}

		virtual double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourcePos = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector TargetPos = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, SourcePos);
			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (TargetPos - SourcePos)) < 0;
			return GetDistSquaredImpl(SourcePos, TargetPos);
		}

		virtual double GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourcePos = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector TargetPos = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, SourcePos);
			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (TargetPos - SourcePos)) < 0;
			return GetDistImpl(SourcePos, TargetPos);
		}
	};

	//
	// Euclidean Distance
	//

	template <EPCGExDistance Source, EPCGExDistance Target>
	class TEuclideanDistances final : public TDistancesBase<Source, Target, TEuclideanDistances<Source, Target>>
	{
		using Super = TDistancesBase<Source, Target, TEuclideanDistances<Source, Target>>;

	public:
		TEuclideanDistances() = default;

		explicit TEuclideanDistances(const bool InOverlapIsZero)
			: Super(InOverlapIsZero)
		{
		}

		FORCEINLINE static double GetDistSquaredStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return FVector::DistSquared(SourcePos, TargetPos);
		}

		FORCEINLINE static double GetDistStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return FVector::Dist(SourcePos, TargetPos);
		}
	};

	//
	// Manhattan Distance
	//

	template <EPCGExDistance Source, EPCGExDistance Target>
	class TManhattanDistances final : public TDistancesBase<Source, Target, TManhattanDistances<Source, Target>>
	{
		using Super = TDistancesBase<Source, Target, TManhattanDistances<Source, Target>>;

	public:
		TManhattanDistances() = default;

		explicit TManhattanDistances(const bool InOverlapIsZero)
			: Super(InOverlapIsZero)
		{
		}

		FORCEINLINE static double GetDistSquaredStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return FMath::Square(DistanceManhattan(SourcePos, TargetPos));
		}

		FORCEINLINE static double GetDistStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return DistanceManhattan(SourcePos, TargetPos);
		}
	};

	//
	// Chebyshev Distance
	//

	template <EPCGExDistance Source, EPCGExDistance Target>
	class TChebyshevDistances final : public TDistancesBase<Source, Target, TChebyshevDistances<Source, Target>>
	{
		using Super = TDistancesBase<Source, Target, TChebyshevDistances<Source, Target>>;

	public:
		TChebyshevDistances() = default;

		explicit TChebyshevDistances(const bool InOverlapIsZero)
			: Super(InOverlapIsZero)
		{
		}

		FORCEINLINE static double GetDistSquaredStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return FMath::Square(DistanceChebyshev(SourcePos, TargetPos));
		}

		FORCEINLINE static double GetDistStatic(const FVector& SourcePos, const FVector& TargetPos)
		{
			return DistanceChebyshev(SourcePos, TargetPos);
		}
	};
}

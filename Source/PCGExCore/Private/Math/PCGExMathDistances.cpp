// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Math/PCGExMathDistances.h"

#include "Data/PCGExPointElements.h"

namespace PCGExMath
{
	template <EPCGExDistance Source, EPCGExDistance Target>
	FVector TDistances<Source, Target>::GetSourceCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return GetSpatializedCenter<Source>(FromPoint, FromCenter, ToCenter);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	FVector TDistances<Source, Target>::GetTargetCenter(const PCGExData::FPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return GetSpatializedCenter<Target>(FromPoint, FromCenter, ToCenter);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	void TDistances<Source, Target>::GetCenters(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		OutSource = GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		OutTarget = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector OutSource = GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		return FVector::DistSquared(OutSource, GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector OutSource = GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		return FVector::Dist(OutSource, GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector SourceOrigin = SourcePoint.GetLocation();
		const FVector OutSource = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
		const FVector OutTarget = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

		bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
		return FVector::DistSquared(OutSource, OutTarget);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDist(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint, bool& bOverlap) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector SourceOrigin = SourcePoint.GetLocation();
		const FVector OutSource = GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
		const FVector OutTarget = GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

		bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
		return FVector::Dist(OutSource, OutTarget);
	}

	template class PCGEXCORE_API TDistances<EPCGExDistance::Center, EPCGExDistance::Center>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::Center, EPCGExDistance::None>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::None>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::None>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::None, EPCGExDistance::Center>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::None, EPCGExDistance::SphereBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::None, EPCGExDistance::BoxBounds>;
	template class PCGEXCORE_API TDistances<EPCGExDistance::None, EPCGExDistance::None>;


	const FDistances* GetDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero)
	{
		// Static cache for each combination
		static TMap<TTuple<EPCGExDistance, EPCGExDistance, bool>, TSharedPtr<FDistances>> Cache;

		const TTuple<EPCGExDistance, EPCGExDistance, bool> Key(Source, Target, bOverlapIsZero);

		// Check if already cached
		if (const TSharedPtr<FDistances>* Found = Cache.Find(Key)) { return Found->Get(); }

		// Create new instance based on parameters
		TSharedPtr<FDistances> NewDistances;

		if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
		{
			NewDistances = MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
		}
		else if (Source == EPCGExDistance::Center)
		{
			if (Target == EPCGExDistance::Center) { NewDistances = MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::Center>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::SphereBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::BoxBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::SphereBounds)
		{
			if (Target == EPCGExDistance::Center) { NewDistances = MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::SphereBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::BoxBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::BoxBounds)
		{
			if (Target == EPCGExDistance::Center) { NewDistances = MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::SphereBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			else if (Target == EPCGExDistance::BoxBounds) { NewDistances = MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}

		// Cache and return
		if (NewDistances) { Cache.Add(Key, NewDistances); }

		return NewDistances.Get();
	}

	const FDistances* GetNoneDistances()
	{
		return GetDistances(EPCGExDistance::None, EPCGExDistance::None);
	}
}

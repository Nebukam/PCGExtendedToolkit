// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExMathDistances.h"
#include "Math/PCGExMathDistancesImpl.h"

namespace PCGExMath
{
#define PCGEX_FOREACH_DISTANCE_PAIR(MACRO) \
    MACRO(EPCGExDistance::Center, EPCGExDistance::Center) \
    MACRO(EPCGExDistance::Center, EPCGExDistance::SphereBounds) \
    MACRO(EPCGExDistance::Center, EPCGExDistance::BoxBounds) \
    MACRO(EPCGExDistance::Center, EPCGExDistance::None) \
    MACRO(EPCGExDistance::SphereBounds, EPCGExDistance::Center) \
    MACRO(EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds) \
    MACRO(EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds) \
    MACRO(EPCGExDistance::SphereBounds, EPCGExDistance::None) \
    MACRO(EPCGExDistance::BoxBounds, EPCGExDistance::Center) \
    MACRO(EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds) \
    MACRO(EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds) \
    MACRO(EPCGExDistance::BoxBounds, EPCGExDistance::None) \
    MACRO(EPCGExDistance::None, EPCGExDistance::Center) \
    MACRO(EPCGExDistance::None, EPCGExDistance::SphereBounds) \
    MACRO(EPCGExDistance::None, EPCGExDistance::BoxBounds) \
    MACRO(EPCGExDistance::None, EPCGExDistance::None)

	const IDistances* GetDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero, const EPCGExDistanceType Type)
	{
		// Static cache for each combination
		static TMap<TTuple<EPCGExDistance, EPCGExDistance, EPCGExDistanceType, bool>, TSharedPtr<IDistances>> Cache;

		const TTuple<EPCGExDistance, EPCGExDistance, EPCGExDistanceType, bool> Key(Source, Target, Type, bOverlapIsZero);

		// Check if already cached
		if (const TSharedPtr<IDistances>* Found = Cache.Find(Key))
		{
			return Found->Get();
		}

		// Create new instance based on parameters
		TSharedPtr<IDistances> NewDistances;

		switch (Type)
		{
		default:
		case EPCGExDistanceType::Euclidian:
			if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
			{
				NewDistances = MakeShared<TEuclideanDistances<EPCGExDistance::None, EPCGExDistance::None>>(bOverlapIsZero);
			}
#define PCGEX_DIST_TPL(_FROM, _TO) \
    else if (Source == _FROM && Target == _TO) { NewDistances = MakeShared<TEuclideanDistances<_FROM, _TO>>(bOverlapIsZero); }
			PCGEX_FOREACH_DISTANCE_PAIR(PCGEX_DIST_TPL)
#undef PCGEX_DIST_TPL
			break;

		case EPCGExDistanceType::Manhattan:
			if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
			{
				NewDistances = MakeShared<TManhattanDistances<EPCGExDistance::None, EPCGExDistance::None>>(bOverlapIsZero);
			}
#define PCGEX_DIST_TPL(_FROM, _TO) \
    else if (Source == _FROM && Target == _TO) { NewDistances = MakeShared<TManhattanDistances<_FROM, _TO>>(bOverlapIsZero); }
			PCGEX_FOREACH_DISTANCE_PAIR(PCGEX_DIST_TPL)
#undef PCGEX_DIST_TPL
			break;

		case EPCGExDistanceType::Chebyshev:
			if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
			{
				NewDistances = MakeShared<TChebyshevDistances<EPCGExDistance::None, EPCGExDistance::None>>(bOverlapIsZero);
			}
#define PCGEX_DIST_TPL(_FROM, _TO) \
    else if (Source == _FROM && Target == _TO) { NewDistances = MakeShared<TChebyshevDistances<_FROM, _TO>>(bOverlapIsZero); }
			PCGEX_FOREACH_DISTANCE_PAIR(PCGEX_DIST_TPL)
#undef PCGEX_DIST_TPL
			break;
		}

		// Cache and return
		if (NewDistances)
		{
			Cache.Add(Key, NewDistances);
		}

		return NewDistances.Get();
	}

#undef PCGEX_FOREACH_DISTANCE_PAIR

	const IDistances* GetNoneDistances()
	{
		return GetDistances(EPCGExDistance::None, EPCGExDistance::None);
	}
}

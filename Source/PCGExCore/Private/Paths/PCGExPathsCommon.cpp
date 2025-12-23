// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsCommon.h"

namespace PCGExPaths
{
	FPathMetrics::FPathMetrics(const FVector& InStart)
	{
		Add(InStart);
	}

	void FPathMetrics::Reset(const FVector& InStart)
	{
		Start = InStart;
		Last = InStart;
		Length = 0;
		Count = 1;
	}

	double FPathMetrics::Add(const FVector& Location)
	{
		if (Length == -1)
		{
			Reset(Location);
			return 0;
		}
		Length += DistToLast(Location);
		Last = Location;
		Count++;
		return Length;
	}

	double FPathMetrics::Add(const FVector& Location, double& OutDistToLast)
	{
		if (Length == -1)
		{
			Reset(Location);
			return 0;
		}
		OutDistToLast = DistToLast(Location);
		Length += OutDistToLast;
		Last = Location;
		Count++;
		return Length;
	}
}

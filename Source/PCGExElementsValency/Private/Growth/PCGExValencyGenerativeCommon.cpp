// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExValencyGenerativeCommon.h"

#pragma region FPCGExBoundsTracker

bool FPCGExBoundsTracker::Overlaps(const FBox& Candidate) const
{
	for (const FBox& Existing : OccupiedBounds)
	{
		if (Existing.Intersect(Candidate))
		{
			return true;
		}
	}
	return false;
}

void FPCGExBoundsTracker::Add(const FBox& Bounds)
{
	OccupiedBounds.Add(Bounds);
}

void FPCGExBoundsTracker::Reset()
{
	OccupiedBounds.Empty();
}

#pragma endregion

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathOutputDetails.h"

bool FPCGExPathOutputDetails::Validate(const int32 NumPathPoints) const
{
	if (NumPathPoints < 2) { return false; }

	if (bRemoveSmallPaths && NumPathPoints < MinPointCount) { return false; }
	if (bRemoveLargePaths && NumPathPoints > MaxPointCount) { return false; }

	return true;
}

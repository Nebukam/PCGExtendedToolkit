// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCPolyline.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier
{
	namespace Offset
	{
		//~ Parallel offset

		/**
		 * Create parallel offset polyline(s).
		 * @param Source Polyline to offset
		 * @param Offset Distance to offset (positive = left of direction, negative = right)
		 * @param Options Offset options
		 * @return Array of resulting polylines
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		TArray<FPolyline> ParallelOffset(FPolyline& Source, double Offset, const FPCGExCCOffsetOptions& Options = FPCGExCCOffsetOptions());
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExPointFilter
{
	const FName OutputFilterLabel = FName("Filter");
	const FName OutputColFilterLabel = FName("C-Filter");
	const FName OutputFilterLabelNode = FName("Vtx Filter");
	const FName OutputFilterLabelEdge = FName("Edge Filter");
	const FName SourceFiltersLabel = FName("Filters");

	const FName SourceFiltersConditionLabel = FName("Conditions Filters");
	const FName SourceKeepConditionLabel = FName("Keep Conditions");
	const FName SourceToggleConditionLabel = FName("Toggle Conditions");
	const FName SourceStartConditionLabel = FName("Start Conditions");
	const FName SourceStopConditionLabel = FName("Stop Conditions");
	const FName SourcePinConditionLabel = FName("Pin Conditions");

	const FName SourcePointFiltersLabel = FName("Point Filters");
	const FName SourceVtxFiltersLabel = FName("Vtx Filters");
	const FName SourceEdgeFiltersLabel = FName("Edge Filters");

	const FName OutputInsideFiltersLabel = FName("Inside");
	const FName OutputOutsideFiltersLabel = FName("Outside");
}
// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

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

namespace PCGExGraph
{
	const FName SourceProbesLabel = TEXT("Probes");
	const FName OutputProbeLabel = TEXT("Probe");

	const FName SourceFilterGenerators = TEXT("Generator Filters");
	const FName SourceFilterConnectables = TEXT("Connectable Filters");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName Tag_PackedClusterEdgeCount_LEGACY = FName(PCGExCommon::PCGExPrefix + TEXT("PackedClusterEdgeCount"));
	const FName Tag_PackedClusterEdgeCount = FName(TEXT("@Data.") + PCGExCommon::PCGExPrefix + TEXT("PackedClusterEdgeCount"));

	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");
	const FName SourcePlotsLabel = TEXT("Plots");

	const FName SourceHeuristicsLabel = TEXT("Heuristics");
	const FName OutputHeuristicsLabel = TEXT("Heuristics");
	const FName OutputModifiersLabel = TEXT("Modifiers");

	const FName SourceVtxFiltersLabel = FName("VtxFilters");
	const FName SourceEdgeFiltersLabel = FName("EdgeFilters");
}

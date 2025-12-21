// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

namespace PCGExGraph
{
	const FName SourceProbesLabel = TEXT("Probes");
	const FName OutputProbeLabel = TEXT("Probe");

	const FName SourceFilterGenerators = TEXT("Generator Filters");
	const FName SourceFilterConnectables = TEXT("Connectable Filters");

	const FName Tag_PackedClusterEdgeCount_LEGACY = FName(PCGExCommon::PCGExPrefix + TEXT("PackedClusterEdgeCount"));
	const FName Tag_PackedClusterEdgeCount = FName(TEXT("@Data.") + PCGExCommon::PCGExPrefix + TEXT("PackedClusterEdgeCount"));

	const FName SourceGoalsLabel = TEXT("Goals");
	const FName SourcePlotsLabel = TEXT("Plots");

}

namespace PCGExStaging
{
	const FName OutputSocketLabel = TEXT("Sockets");
}


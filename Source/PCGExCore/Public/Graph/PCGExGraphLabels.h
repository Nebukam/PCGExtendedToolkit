// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

namespace PCGExGraph
{
	namespace Labels
	{
		const FName SourcePickersLabel = TEXT("Pickers");

		const FName SourceEdgesLabel = TEXT("Edges");
		const FName OutputEdgesLabel = TEXT("Edges");
		const FName OutputSitesLabel = TEXT("Sites");

		const FName OutputKeptEdgesLabel = TEXT("Kept Edges");
		const FName OutputRemovedEdgesLabel = TEXT("Removed Edges");

		const FName SourcePackedClustersLabel = TEXT("Packed Clusters");
		const FName SourceEdgeSortingRules = TEXT("Direction Sorting");
		const FName OutputPackedClustersLabel = TEXT("Packed Clusters");

		const FName Attr_PCGExEdgeIdx = FName(PCGExCommon::PCGExPrefix + TEXT("EData"));
		const FName Attr_PCGExVtxIdx = FName(PCGExCommon::PCGExPrefix + TEXT("VData"));

		const FName Tag_PCGExCluster = FName(PCGExCommon::PCGExPrefix + TEXT("Cluster"));
		const FString TagStr_PCGExCluster = Tag_PCGExCluster.ToString();

		const FName Tag_PCGExVtx = FName(PCGExCommon::PCGExPrefix + TEXT("Vtx"));
		const FString TagStr_PCGExVtx = Tag_PCGExVtx.ToString();
		const FName Tag_PCGExEdges = FName(PCGExCommon::PCGExPrefix + TEXT("Edges"));
		const FString TagStr_PCGExEdges = Tag_PCGExEdges.ToString();

		PCGEX_CTX_STATE(State_ReadyForNextEdges)

		const TSet<FName> ProtectedClusterAttributes = {Attr_PCGExEdgeIdx, Attr_PCGExVtxIdx};
	}
}

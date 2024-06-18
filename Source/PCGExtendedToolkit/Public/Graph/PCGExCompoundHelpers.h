// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGraph.h"
#include "PCGExEdge.h"
#include "PCGExIntersections.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FCompoundProcessor
	{
		FPCGExPointsProcessorContext* Context = nullptr;
		
		FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

		bool bDoPointEdge = false;
		FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;
		bool bUseCustomPointEdgeBlending = false;
		FPCGExBlendingSettings CustomPointEdgeBlendingSettings;

		bool bDoEdgeEdge = false;
		FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;
		bool bUseCustomEdgeEdgeBlending = false;
		FPCGExBlendingSettings CustomEdgeEdgeBlendingSettings;

		FPCGExGraphBuilderSettings GraphBuilderSettings;

		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
		PCGExData::FPointIO* CompoundPoints = nullptr;
		PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;
		
		explicit FCompoundProcessor(
			FPCGExPointsProcessorContext* InContext,
			FPCGExPointPointIntersectionSettings PointPointIntersectionSettings,
			FPCGExBlendingSettings InDefaultPointsBlending,
			FPCGExBlendingSettings InDefaultEdgesBlending);

		~FCompoundProcessor();

		void InitPointEdge(
			const FPCGExPointEdgeIntersectionSettings& InSettings,
			const bool bUseCustom = false,
			const FPCGExBlendingSettings* InOverride = nullptr);

		void InitEdgeEdge(
			const FPCGExEdgeEdgeIntersectionSettings& InSettings,
			const bool bUseCustom = false,
			const FPCGExBlendingSettings* InOverride = nullptr);

		template <class BuildGraphFunc>
		void StartProcessing(
			PCGExGraph::FCompoundGraph* InCompoundGraph,
			PCGExData::FPointIO* InCompoundPoints,
			const FPCGExGraphBuilderSettings& InBuilderSettings,
			BuildGraphFunc&& BuildGraph)
		{
			bRunning = true;

			GraphMetadataSettings.Grab(Context, PointPointIntersectionSettings);
			GraphMetadataSettings.Grab(Context, PointEdgeIntersectionSettings);
			GraphMetadataSettings.Grab(Context, EdgeEdgeIntersectionSettings);

			CompoundGraph = InCompoundGraph;
			CompoundPoints = InCompoundPoints;

			GraphBuilder = new PCGExGraph::FGraphBuilder(*CompoundPoints, &InBuilderSettings, 4);

			BuildGraph(GraphBuilder);

			/*
			TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
			CompoundGraph->GetUniqueEdges(UniqueEdges);
			CompoundGraph->WriteMetadata(GraphBuilder->Graph->NodeMetadata);

			GraphBuilder->Graph->InsertEdges(UniqueEdges, -1); //TODO : valid IOIndex from CompoundGraph
			UniqueEdges.Empty();
			*/
			
			if (bDoPointEdge) { FindPointEdgeIntersections(); }
			else if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else { Context->SetState(PCGExGraph::State_WritingClusters); }
		}
		
		bool Execute();
		bool IsRunning() const;

	protected:
		bool bRunning = false;
		
		FPCGExBlendingSettings DefaultPointsBlendingSettings;
		FPCGExBlendingSettings DefaultEdgesBlendingSettings;
		
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
		PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
		PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;
		PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;
		
		void FindPointEdgeIntersections();
		void FindEdgeEdgeIntersections();
	};
}

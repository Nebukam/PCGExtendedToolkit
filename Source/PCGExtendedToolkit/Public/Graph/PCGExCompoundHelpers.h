// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGraph.h"
#include "PCGExEdge.h"
#include "PCGExIntersections.h"
#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FCompoundProcessor
	{
		FPCGExPointsProcessorContext* Context = nullptr;

		FPCGExPointPointIntersectionDetails PointPointIntersectionDetails;

		bool bDoPointEdge = false;
		FPCGExPointEdgeIntersectionDetails PointEdgeIntersectionDetails;
		bool bUseCustomPointEdgeBlending = false;
		FPCGExBlendingDetails CustomPointEdgeBlendingDetails;

		bool bDoEdgeEdge = false;
		FPCGExEdgeEdgeIntersectionDetails EdgeEdgeIntersectionDetails;
		bool bUseCustomEdgeEdgeBlending = false;
		FPCGExBlendingDetails CustomEdgeEdgeBlendingDetails;

		FPCGExGraphBuilderDetails GraphBuilderDetails;

		FCompoundGraph* CompoundGraph = nullptr;
		PCGExData::FFacade* CompoundFacade = nullptr;
		PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

		explicit FCompoundProcessor(
			FPCGExPointsProcessorContext* InContext,
			FPCGExPointPointIntersectionDetails PointPointIntersectionDetails,
			FPCGExBlendingDetails InDefaultPointsBlending,
			FPCGExBlendingDetails InDefaultEdgesBlending);

		~FCompoundProcessor();

		void InitPointEdge(
			const FPCGExPointEdgeIntersectionDetails& InDetails,
			const bool bUseCustom = false,
			const FPCGExBlendingDetails* InOverride = nullptr);

		void InitEdgeEdge(
			const FPCGExEdgeEdgeIntersectionDetails& InDetails,
			const bool bUseCustom = false,
			const FPCGExBlendingDetails* InOverride = nullptr);

		template <class BuildGraphFunc>
		void StartProcessing(
			FCompoundGraph* InCompoundGraph,
			PCGExData::FFacade* InCompoundFacade,
			const FPCGExGraphBuilderDetails& InBuilderDetails,
			BuildGraphFunc&& BuildGraph)
		{
			bRunning = true;

			GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
			GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
			GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);

			CompoundGraph = InCompoundGraph;
			CompoundFacade = InCompoundFacade;

			GraphBuilder = new FGraphBuilder(CompoundFacade->Source, &InBuilderDetails, 4);

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
			else { Context->SetState(State_WritingClusters); }
		}

		bool Execute();
		bool IsRunning() const;

	protected:
		bool bRunning = false;

		FPCGExBlendingDetails DefaultPointsBlendingDetails;
		FPCGExBlendingDetails DefaultEdgesBlendingDetails;

		FGraphBuilder* GraphBuilder = nullptr;

		FGraphMetadataDetails GraphMetadataDetails;
		FPointEdgeIntersections* PointEdgeIntersections = nullptr;
		FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;
		PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;

		void FindPointEdgeIntersections();
		void FindEdgeEdgeIntersections();
	};
}

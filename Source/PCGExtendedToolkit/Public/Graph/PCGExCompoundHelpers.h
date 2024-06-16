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

		PCGExData::FPointIO* CompoundedPoints = nullptr;
		FCompoundGraph* CompoundGraph = nullptr;
		PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

		// Intersection settings
		FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

		// Blending settings
		FPCGExBlendingSettings DefaultPointsBlendingSettings;

		explicit FCompoundProcessor(
			FPCGExPointsProcessorContext* InContext,
			FBox Bounds,
			const FPCGExPointPointIntersectionSettings& InPointPointIntersectionSettings,
			FPCGExBlendingSettings InDefaultPointsBlendingSettings);
	
		
	};

	struct PCGEXTENDEDTOOLKIT_API FIntersectionsHandler
	{
		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		// Intersection settings
		bool bFindPointEdgeIntersections = false;
		FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;

		bool bFindEdgeEdgeIntersections = false;
		FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

		PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;

		// Blending
		FPCGExBlendingSettings DefaultEdgesBlendingSettings;

		bool bUseCustomPointEdgeBlending = false;
		FPCGExBlendingSettings CustomPointEdgeBlendingSettings;

		bool bUseCustomEdgeEdgeBlending = false;
		FPCGExBlendingSettings CustomEdgeEdgeBlendingSettings;

		PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;

		PCGExData::FPointIO* ConsolidatedPoints = nullptr;

		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;

		FPCGExGraphBuilderSettings GraphBuilderSettings;
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
		PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;

		explicit FIntersectionsHandler(FPCGExPointsProcessorContext* InContext):
			Context(InContext)
		{
		}

		~FIntersectionsHandler()
		{
		}

		void Init();
	};
}

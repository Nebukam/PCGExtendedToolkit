// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCompoundHelpers.h"

namespace PCGExGraph
{
	FCompoundProcessor::FCompoundProcessor(
		FPCGExPointsProcessorContext* InContext,
		FBox Bounds,
		const FPCGExPointPointIntersectionSettings& InPointPointIntersectionSettings,
		FPCGExBlendingSettings InDefaultPointsBlendingSettings) :
		Context(InContext),
		PointPointIntersectionSettings(InPointPointIntersectionSettings),
		DefaultPointsBlendingSettings(InDefaultPointsBlendingSettings)
	{
		CompoundGraph = new PCGExGraph::FCompoundGraph(PointPointIntersectionSettings.FuseSettings, Bounds);

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(&DefaultPointsBlendingSettings);
		CompoundPointsBlender->AddSources(*Context->MainPoints);
	}

	void FIntersectionsHandler::Init()
	{
		EdgeEdgeIntersectionSettings.ComputeDot();
		/*
				GraphMetadataSettings.Grab(Context, PointPointIntersectionSettings);
				GraphMetadataSettings.Grab(Context, PointEdgeIntersectionSettings);
				GraphMetadataSettings.Grab(Context, EdgeEdgeIntersectionSettings);
		*/
	}
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGraph.h"
#include "PCGExIntersections.h"
#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"


namespace PCGExGraph
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCompoundProcessor : TSharedFromThis<FCompoundProcessor>
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

		TSharedPtr<FCompoundGraph> CompoundGraph;
		TSharedPtr<PCGExData::FFacade> CompoundFacade;
		TSharedPtr<PCGExDataBlending::FCompoundBlender> CompoundPointsBlender;

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

		bool StartExecution(
			const TSharedPtr<FCompoundGraph>& InCompoundGraph,
			const TSharedPtr<PCGExData::FFacade>& InCompoundFacade,
			const TArray<TSharedPtr<PCGExData::FFacade>>& InFacades,
			const FPCGExGraphBuilderDetails& InBuilderDetails,
			const FPCGExCarryOverDetails* InCarryOverDetails);

		void InternalStartExecution();
		bool Execute();

	protected:
		bool bRunning = false;

		int32 NewEdgesNum = 0;

		FPCGExBlendingDetails DefaultPointsBlendingDetails;
		FPCGExBlendingDetails DefaultEdgesBlendingDetails;

		TSharedPtr<FGraphBuilder> GraphBuilder;

		FGraphMetadataDetails GraphMetadataDetails;
		TSharedPtr<FPointEdgeIntersections> PointEdgeIntersections;
		TSharedPtr<FEdgeEdgeIntersections> EdgeEdgeIntersections;
		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;

		void FindPointEdgeIntersections();
		void FindPointEdgeIntersectionsFound();
		void OnPointEdgeIntersectionsComplete();

		void FindEdgeEdgeIntersections();
		void OnEdgeEdgeIntersectionsFound();
		void OnEdgeEdgeIntersectionsComplete();
		void WriteClusters();
	};
}

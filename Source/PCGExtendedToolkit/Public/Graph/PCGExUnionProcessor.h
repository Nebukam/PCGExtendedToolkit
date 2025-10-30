﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraph.h"
#include "PCGExIntersections.h"
#include "PCGExPointsProcessor.h"
#include "Details/PCGExDetailsIntersection.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExMetadataBlender.h"

namespace PCGExGraph
{
	class PCGEXTENDEDTOOLKIT_API FUnionProcessor : public TSharedFromThis<FUnionProcessor>
	{
		bool bCompilingFinalGraph = false;
		TSharedPtr<PCGExDetails::FDistances> Distances;

	public:
		FPCGExPointsProcessorContext* Context = nullptr;

		TSharedRef<PCGExData::FFacade> UnionDataFacade;
		TSharedPtr<FUnionGraph> UnionGraph;
		const TArray<TSharedRef<PCGExData::FFacade>>* SourceEdgesIO = nullptr;

		FPCGExPointPointIntersectionDetails PointPointIntersectionDetails;
		const FPCGExCarryOverDetails* VtxCarryOverDetails = nullptr;
		const FPCGExCarryOverDetails* EdgesCarryOverDetails = nullptr;

		bool bDoPointEdge = false;
		FPCGExPointEdgeIntersectionDetails PointEdgeIntersectionDetails;
		bool bUseCustomPointEdgeBlending = false;
		FPCGExBlendingDetails CustomPointEdgeBlendingDetails;

		bool bDoEdgeEdge = false;
		FPCGExEdgeEdgeIntersectionDetails EdgeEdgeIntersectionDetails;
		bool bUseCustomEdgeEdgeBlending = false;
		FPCGExBlendingDetails CustomEdgeEdgeBlendingDetails;

		FPCGExGraphBuilderDetails GraphBuilderDetails;


		TSharedPtr<PCGExDataBlending::IUnionBlender> UnionBlender;

		explicit FUnionProcessor(
			FPCGExPointsProcessorContext* InContext,
			TSharedRef<PCGExData::FFacade> InUnionDataFacade,
			TSharedRef<FUnionGraph> InUnionGraph,
			FPCGExPointPointIntersectionDetails PointPointIntersectionDetails,
			FPCGExBlendingDetails InDefaultPointsBlending,
			FPCGExBlendingDetails InDefaultEdgesBlending);

		~FUnionProcessor();

		void InitPointEdge(
			const FPCGExPointEdgeIntersectionDetails& InDetails,
			const bool bUseCustom = false,
			const FPCGExBlendingDetails* InOverride = nullptr);

		void InitEdgeEdge(
			const FPCGExEdgeEdgeIntersectionDetails& InDetails,
			const bool bUseCustom = false,
			const FPCGExBlendingDetails* InOverride = nullptr);

		bool StartExecution(
			const TArray<TSharedRef<PCGExData::FFacade>>& InFacades,
			const FPCGExGraphBuilderDetails& InBuilderDetails);

		bool Execute();

	protected:
		bool bRunning = false;

		int32 PENum = 0;
		int32 EENum = 0;

		void OnNodesProcessingComplete();
		void InternalStartExecution();

		FPCGExGraphBuilderDetails BuilderDetails;
		FPCGExBlendingDetails DefaultPointsBlendingDetails;
		FPCGExBlendingDetails DefaultEdgesBlendingDetails;

		TSharedPtr<FGraphBuilder> GraphBuilder;

		FGraphMetadataDetails GraphMetadataDetails;

		TSharedPtr<FPointEdgeIntersections> PointEdgeIntersections;

		TSharedPtr<PCGExMT::TScopedPtr<FEdgeEdgeIntersections>> ScopedEdgeEdgeIntersections;
		TSharedPtr<FEdgeEdgeIntersections> EdgeEdgeIntersections;

		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;

		void FindPointEdgeIntersections();
		void OnPointEdgeIntersectionsFound();
		void OnPointEdgeIntersectionsComplete();

		void FindEdgeEdgeIntersections();
		void OnEdgeEdgeIntersectionsFound();
		void OnEdgeEdgeIntersectionsComplete();
		void CompileFinalGraph();
	};
}

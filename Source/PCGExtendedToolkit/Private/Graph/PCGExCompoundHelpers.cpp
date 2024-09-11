// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCompoundHelpers.h"

namespace PCGExGraph
{
	// Need :
	// - GraphBuilder
	// - CompoundGraph
	// - CompoundPoints

	// - PointEdgeIntersectionSettings


	FCompoundProcessor::FCompoundProcessor(
		FPCGExPointsProcessorContext* InContext,
		FPCGExPointPointIntersectionDetails InPointPointIntersectionSettings,
		FPCGExBlendingDetails InDefaultPointsBlending,
		FPCGExBlendingDetails InDefaultEdgesBlending):
		Context(InContext),
		PointPointIntersectionDetails(InPointPointIntersectionSettings),
		DefaultPointsBlendingDetails(InDefaultPointsBlending),
		DefaultEdgesBlendingDetails(InDefaultEdgesBlending)
	{
	}

	FCompoundProcessor::~FCompoundProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(CompoundPointsBlender)
		PCGEX_DELETE(PointEdgeIntersections)
		PCGEX_DELETE(EdgeEdgeIntersections)
		PCGEX_DELETE(MetadataBlender)
	}

	void FCompoundProcessor::InitPointEdge(
		const FPCGExPointEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoPointEdge = true;
		PointEdgeIntersectionDetails = InDetails;
		bUseCustomPointEdgeBlending = bUseCustom;
		if (InOverride) { CustomPointEdgeBlendingDetails = *InOverride; }
	}

	void FCompoundProcessor::InitEdgeEdge(
		const FPCGExEdgeEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoEdgeEdge = true;
		EdgeEdgeIntersectionDetails = InDetails;
		bUseCustomEdgeEdgeBlending = bUseCustom;
		if (InOverride) { CustomEdgeEdgeBlendingDetails = *InOverride; }
	}

	bool FCompoundProcessor::StartExecution(
		FCompoundGraph* InCompoundGraph,
		PCGExData::FFacade* InCompoundFacade,
		const TArray<PCGExData::FFacade*>& InFacades,
		const FPCGExGraphBuilderDetails& InBuilderDetails,
		const FPCGExCarryOverDetails* InCarryOverDetails)
	{
		CompoundGraph = InCompoundGraph;
		CompoundFacade = InCompoundFacade;

		const int32 NumCompoundNodes = CompoundGraph->Nodes.Num();
		if (NumCompoundNodes == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Compound graph is empty. Something is likely corrupted."));
			return false;
		}

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(&DefaultPointsBlendingDetails, InCarryOverDetails);

		TArray<FPCGPoint>& MutablePoints = CompoundFacade->GetOut()->GetMutablePoints();
		CompoundFacade->Source->InitializeNum(NumCompoundNodes, true);
		CompoundPointsBlender->AddSources(InFacades);
		CompoundPointsBlender->PrepareMerge(CompoundFacade, CompoundGraph->PointsCompounds, nullptr); // TODO : Check if we want to ignore specific attributes

		Context->SetAsyncState(State_ProcessingCompound);

		PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), ProcessNodesGroup)
		ProcessNodesGroup->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_DELETE(CompoundPointsBlender)

				CompoundFacade->Write(Context->GetAsyncManager(), true);
				Context->SetAsyncState(PCGExMT::State_CompoundWriting);

				bRunning = true;

				GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
				GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
				GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);

				GraphBuilder = new FGraphBuilder(CompoundFacade->Source, &InBuilderDetails, 4);

				TSet<uint64> UniqueEdges;
				CompoundGraph->GetUniqueEdges(UniqueEdges);
				CompoundGraph->WriteMetadata(GraphBuilder->Graph->NodeMetadata);

				GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);

				InternalStartExecution();
			});

		ProcessNodesGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				FCompoundNode* CompoundNode = CompoundGraph->Nodes[Index];
				PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
				MutablePoints[Index] = CompoundNode->Point; // Copy "original" point properties, in case  there's only one

				FPCGPoint& Point = MutablePoints[Index];
				Point.MetadataEntry = Key; // Restore key

				Point.Transform.SetLocation(
					CompoundNode->UpdateCenter(CompoundGraph->PointsCompounds, Context->MainPoints));
				CompoundPointsBlender->MergeSingle(Index, PCGExDetails::GetDistanceDetails(PointPointIntersectionDetails));
			}, NumCompoundNodes, 256, false, false);


		return true;
	}

	void FCompoundProcessor::InternalStartExecution()
	{
		if (bDoPointEdge)
		{
			FindEdgeEdgeIntersections();
			return;
		}

		if (bDoEdgeEdge)
		{
			FindEdgeEdgeIntersections();
			return;
		}

		WriteClusters();
	}

	bool FCompoundProcessor::Execute()
	{
		if (!bRunning) { return false; }

		if (Context->IsState(State_ProcessingCompound)) { return false; }

		if (Context->IsState(State_ProcessingPointEdgeIntersections))
		{
			PCGEX_ASYNC_WAIT

			PCGEX_DELETE(MetadataBlender)

			if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else { WriteClusters(); }
		}

		if (Context->IsState(State_ProcessingEdgeEdgeIntersections))
		{
			PCGEX_ASYNC_WAIT

			PCGEX_DELETE(MetadataBlender)

			WriteClusters();
		}

		if (Context->IsState(State_WritingClusters))
		{
			PCGEX_ASYNC_WAIT

			if (!GraphBuilder->bCompiledSuccessfully)
			{
				CompoundFacade->Source->InitializeOutput(PCGExData::EInit::NoOutput);
				return true;
			}

			GraphBuilder->Write();
			return true;
		}

		return true;
	}

	void FCompoundProcessor::FindPointEdgeIntersections()
	{
		PointEdgeIntersections = new FPointEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingPointEdgeIntersections);

		PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), FindPointEdgeGroup)
		FindPointEdgeGroup->SetOnCompleteCallback(
			[&]()
			{
				PointEdgeIntersections->Insert(); // TODO : Async?
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomPointEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomPointEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out);

				PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), BlendPointEdgeGroup)
				BlendPointEdgeGroup->SetOnCompleteCallback(
					[&]()
					{
						if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }
						PCGEX_DELETE(PointEdgeIntersections)
					});

				BlendPointEdgeGroup->StartRanges(
					[&](const int32 Index, const int32 Count, const int32 LoopIdx)
					{
						// TODO
					}, PointEdgeIntersections->Edges.Num(), 256);
			});

		FindPointEdgeGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
				if (!Edge.bValid) { return; }
				FindCollinearNodes(PointEdgeIntersections, Index, CompoundFacade->Source->GetOut());
			},
			GraphBuilder->Graph->Edges.Num(), 256);
	}

	void FCompoundProcessor::FindEdgeEdgeIntersections()
	{
		EdgeEdgeIntersections = new FEdgeEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &EdgeEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingEdgeEdgeIntersections);

		PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), FindEdgeEdgeGroup)
		FindEdgeEdgeGroup->SetOnCompleteCallback(
			[&]()
			{
				EdgeEdgeIntersections->Insert(); // TODO : Async?
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomPointEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomEdgeEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out);

				PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), BlendEdgeEdgeGroup)
				BlendEdgeEdgeGroup->SetOnCompleteCallback(
					[&]()
					{
						if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }
						PCGEX_DELETE(EdgeEdgeIntersections)
					});

				BlendEdgeEdgeGroup->StartRanges(
					[&](const int32 Index, const int32 Count, const int32 LoopIdx)
					{
						EdgeEdgeIntersections->BlendIntersection(Index, MetadataBlender);
					}, EdgeEdgeIntersections->Crossings.Num(), 256);
			});

		FindEdgeEdgeGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
				if (!Edge.bValid) { return; }
				FindOverlappingEdges(EdgeEdgeIntersections, Index);
			},
			GraphBuilder->Graph->Edges.Num(), 256);
	}

	void FCompoundProcessor::WriteClusters()
	{
		Context->SetAsyncState(State_WritingClusters);
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), &GraphMetadataDetails);
	}
}

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

#pragma region PointEdge

	void FCompoundProcessor::FindPointEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindPointEdgeIntersections);

		PointEdgeIntersections = new FPointEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingPointEdgeIntersections);

		PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), FindPointEdgeGroup)

		FindPointEdgeGroup->SetOnCompleteCallback([&]() { FindPointEdgeIntersectionsFound(); });
		FindPointEdgeGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
				if (!Edge.bValid) { return; }
				FindCollinearNodes(PointEdgeIntersections, Index, CompoundFacade->Source->GetOut());
			},
			GraphBuilder->Graph->Edges.Num(), 256);
	}

	void FCompoundProcessor::FindPointEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindPointEdgeIntersectionsFound);

		PCGEX_ASYNC_GROUP_CHECKED(Context->GetAsyncManager(), SortCrossingsGroup)

		SortCrossingsGroup->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
				{
					FPointEdgeProxy& PointEdgeProxy = PointEdgeIntersections->Edges[i];
					const int32 CollinearNum = PointEdgeProxy.CollinearPoints.Num();

					if (!CollinearNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&NewEdgesNum, (CollinearNum + 1));

					FIndexedEdge& SplitEdge = GraphBuilder->Graph->Edges[PointEdgeProxy.EdgeIndex];
					SplitEdge.bValid = false; // Invalidate existing edge
					PointEdgeProxy.CollinearPoints.Sort([](const FPESplit& A, const FPESplit& B) { return A.Time < B.Time; });
				}
			});


		SortCrossingsGroup->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_ASYNC_GROUP_CHECKED(Context->GetAsyncManager(), BlendPointEdgeGroup)

				GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
				NewEdgesNum = 0;

				PointEdgeIntersections->Insert(); // TODO : Async?
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomPointEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomPointEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out);

				BlendPointEdgeGroup->SetOnCompleteCallback([&]() { FindPointEdgeIntersectionsComplete(); });
				BlendPointEdgeGroup->SetOnIterationRangeStartCallback(
					[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							// TODO
						}
					});
				BlendPointEdgeGroup->PrepareRangesOnly(PointEdgeIntersections->Edges.Num(), 256);
			});

		SortCrossingsGroup->PrepareRangesOnly(PointEdgeIntersections->Edges.Num(), 256);

		///
	}

	void FCompoundProcessor::FindPointEdgeIntersectionsComplete()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindPointEdgeIntersectionsComplete);

		if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }
		PCGEX_DELETE(PointEdgeIntersections)
	}

#pragma endregion

#pragma region EdgeEdge

	void FCompoundProcessor::FindEdgeEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindEdgeEdgeIntersections);

		EdgeEdgeIntersections = new FEdgeEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &EdgeEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingEdgeEdgeIntersections);

		PCGEX_ASYNC_GROUP(Context->GetAsyncManager(), FindEdgeEdgeGroup)

		FindEdgeEdgeGroup->SetOnCompleteCallback([&]() { OnEdgeEdgeIntersectionsFound(); });
		FindEdgeEdgeGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
				if (!Edge.bValid) { return; }
				FindOverlappingEdges(EdgeEdgeIntersections, Index);
			},
			GraphBuilder->Graph->Edges.Num(), 256);
	}

	void FCompoundProcessor::OnEdgeEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::OnEdgeEdgeIntersectionsFound);

		PCGEX_ASYNC_GROUP_CHECKED(Context->GetAsyncManager(), SortCrossingsGroup)

		// Insert new nodes
		EdgeEdgeIntersections->InsertNodes(); // TODO : Async?

		SortCrossingsGroup->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
				{
					FEdgeEdgeProxy& EdgeProxy = EdgeEdgeIntersections->Edges[i];
					const int32 IntersectionsNum = EdgeProxy.Intersections.Num();

					if (!IntersectionsNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&NewEdgesNum, (IntersectionsNum + 1));

					GraphBuilder->Graph->Edges[EdgeProxy.EdgeIndex].bValid = false; // Invalidate existing edge

					EdgeProxy.Intersections.Sort(
						[&](const FEECrossing& A, const FEECrossing& B)
						{
							return A.GetTime(EdgeProxy.EdgeIndex) > B.GetTime(EdgeProxy.EdgeIndex);
						});
				}
			});

		SortCrossingsGroup->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_ASYNC_GROUP_CHECKED(Context->GetAsyncManager(), BlendEdgeEdgeGroup)

				GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
				NewEdgesNum = 0;

				// TODO : Multi-thread by reserving the future edges in metadata and others.
				// Edge count is known and uniqueness is known in advance, we just need the number of edges, which we have.
				// Do set num instead of reserve, Graph has an InsertEdges that return the starting addition index.
				// use range prep to cache these and rebuild metadata and everything then.

				EdgeEdgeIntersections->InsertEdges(); // TODO : Async?
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomEdgeEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomEdgeEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out);

				BlendEdgeEdgeGroup->SetOnCompleteCallback([&]() { OnEdgeEdgeIntersectionsComplete(); });
				BlendEdgeEdgeGroup->SetOnIterationRangeStartCallback(
					[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							EdgeEdgeIntersections->BlendIntersection(i, MetadataBlender);
						}
					});
				BlendEdgeEdgeGroup->PrepareRangesOnly(EdgeEdgeIntersections->Crossings.Num(), 256);
			});

		SortCrossingsGroup->PrepareRangesOnly(EdgeEdgeIntersections->Edges.Num(), 256);
	}

	void FCompoundProcessor::OnEdgeEdgeIntersectionsComplete()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::OnEdgeEdgeIntersectionsComplete);

		if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }
		PCGEX_DELETE(EdgeEdgeIntersections)
	}

#pragma endregion

	void FCompoundProcessor::WriteClusters()
	{
		Context->SetAsyncState(State_WritingClusters);
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), &GraphMetadataDetails);
	}
}

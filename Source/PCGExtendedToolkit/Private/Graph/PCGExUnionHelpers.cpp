// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnionHelpers.h"

namespace PCGExGraph
{
	FUnionProcessor::FUnionProcessor(
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

	FUnionProcessor::~FUnionProcessor()
	{
	}

	void FUnionProcessor::InitPointEdge(
		const FPCGExPointEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoPointEdge = true;
		PointEdgeIntersectionDetails = InDetails;
		bUseCustomPointEdgeBlending = bUseCustom;
		if (InOverride) { CustomPointEdgeBlendingDetails = *InOverride; }
	}

	void FUnionProcessor::InitEdgeEdge(
		const FPCGExEdgeEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoEdgeEdge = true;
		EdgeEdgeIntersectionDetails = InDetails;
		bUseCustomEdgeEdgeBlending = bUseCustom;
		if (InOverride) { CustomEdgeEdgeBlendingDetails = *InOverride; }
	}

	bool FUnionProcessor::StartExecution(
		const TSharedPtr<FUnionGraph>& InUnionGraph,
		const TSharedPtr<PCGExData::FFacade>& InUnionFacade,
		const TArray<TSharedPtr<PCGExData::FFacade>>& InFacades,
		const FPCGExGraphBuilderDetails& InBuilderDetails,
		const FPCGExCarryOverDetails* InCarryOverDetails)
	{
		UnionGraph = InUnionGraph;
		UnionFacade = InUnionFacade;

		const int32 NumUnionNodes = UnionGraph->Nodes.Num();
		if (NumUnionNodes == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Union graph is empty. Something is likely corrupted."));
			return false;
		}

		UnionPointsBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&DefaultPointsBlendingDetails, InCarryOverDetails);

		TArray<FPCGPoint>& MutablePoints = UnionFacade->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumUnionNodes);

		UnionPointsBlender->AddSources(InFacades);
		UnionPointsBlender->PrepareMerge(UnionFacade, UnionGraph->PointsUnion, nullptr); // TODO : Check if we want to ignore specific attributes // Answer : yes, cluster IDs etc

		Context->SetAsyncState(State_ProcessingUnion);

		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), ProcessNodesGroup)
		ProcessNodesGroup->OnCompleteCallback =
			[&]()
			{
				UnionPointsBlender.Reset();

				UnionFacade->Write(Context->GetAsyncManager());
				Context->SetAsyncState(PCGEx::State_UnionWriting);

				bRunning = true;

				GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
				GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
				GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);

				GraphBuilder = MakeShared<FGraphBuilder>(UnionFacade, &InBuilderDetails, 4);

				//TSet<uint64> UniqueEdges;
				//UnionGraph->GetUniqueEdges(UniqueEdges);
				//GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
				TArray<FIndexedEdge> UniqueEdges;
				UnionGraph->GetUniqueEdges(UniqueEdges);
				GraphBuilder->Graph->InsertEdges(UniqueEdges);

				PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), WriteMetadataTask);
				WriteMetadataTask->OnCompleteCallback = [&]() { InternalStartExecution(); };
				WriteMetadataTask->AddSimpleCallback([&]() { UnionGraph->WriteNodeMetadata(GraphBuilder->Graph->NodeMetadata); });
				WriteMetadataTask->AddSimpleCallback([&]() { UnionGraph->WriteEdgeMetadata(GraphBuilder->Graph->EdgeMetadata); });
				WriteMetadataTask->StartSimpleCallbacks();
			};

		ProcessNodesGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			FUnionNode* UnionNode = UnionGraph->Nodes[Index].Get();
			PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
			MutablePoints[Index] = UnionNode->Point; // Copy "original" point properties, in case  there's only one

			FPCGPoint& Point = MutablePoints[Index];
			Point.MetadataEntry = Key; // Restore key

			Point.Transform.SetLocation(
				UnionNode->UpdateCenter(UnionGraph->PointsUnion.Get(), Context->MainPoints.Get()));
			UnionPointsBlender->MergeSingle(Index, PCGExDetails::GetDistanceDetails(PointPointIntersectionDetails));
		};

		ProcessNodesGroup->StartIterations(NumUnionNodes, GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize, false, false);


		return true;
	}

	void FUnionProcessor::InternalStartExecution()
	{
		if (bDoPointEdge)
		{
			FindPointEdgeIntersections();
			return;
		}

		if (bDoEdgeEdge)
		{
			FindEdgeEdgeIntersections();
			return;
		}

		WriteClusters();
	}

	bool FUnionProcessor::Execute()
	{
		if (!bRunning) { return false; }

		if (Context->IsState(State_ProcessingUnion)) { return false; }

		PCGEX_ON_ASYNC_STATE_READY(State_ProcessingPointEdgeIntersections)
		{
			if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else { WriteClusters(); }
			return false;
		}

		PCGEX_ON_ASYNC_STATE_READY(State_ProcessingEdgeEdgeIntersections)
		{
			WriteClusters();
			return false;
		}

		PCGEX_ON_ASYNC_STATE_READY(State_WritingClusters)
		{
			return true;
		}

		return true;
	}

#pragma region PointEdge

	void FUnionProcessor::FindPointEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindPointEdgeIntersections);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), FindPointEdgeGroup)

		PointEdgeIntersections = MakeShared<FPointEdgeIntersections>(
			GraphBuilder->Graph, UnionGraph, UnionFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingPointEdgeIntersections);

		FindPointEdgeGroup->OnCompleteCallback = [&]() { FindPointEdgeIntersectionsFound(); };
		FindPointEdgeGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
			if (!Edge.bValid) { return; }
			FindCollinearNodes(PointEdgeIntersections.Get(), Index, UnionFacade->Source->GetOut());
		};
		FindPointEdgeGroup->StartIterations(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::FindPointEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindPointEdgeIntersectionsFound);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		SortCrossingsGroup->OnIterationRangeStartCallback =
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
			};


		SortCrossingsGroup->OnCompleteCallback =
			[&]()
			{
				PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), BlendPointEdgeGroup)

				GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
				NewEdgesNum = 0;

				PointEdgeIntersections->Insert(); // TODO : Async?
				UnionFacade->Source->CleanupKeys();

				if (bUseCustomPointEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomPointEdgeBlendingDetails); }
				else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(UnionFacade.ToSharedRef(), PCGExData::ESource::Out);

				BlendPointEdgeGroup->OnCompleteCallback = [&]() { OnPointEdgeIntersectionsComplete(); };
				BlendPointEdgeGroup->OnIterationRangeStartCallback =
					[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						if (!MetadataBlender) { return; }
						const TSharedRef<PCGExDataBlending::FMetadataBlender> Blender = MetadataBlender.ToSharedRef();

						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							// TODO
						}
					};
				BlendPointEdgeGroup->PrepareRangesOnly(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
			};

		SortCrossingsGroup->PrepareRangesOnly(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		///
	}

	void FUnionProcessor::OnPointEdgeIntersectionsComplete()
	{
		if (MetadataBlender) { UnionFacade->Write(Context->GetAsyncManager()); }
	}

#pragma endregion

#pragma region EdgeEdge

	void FUnionProcessor::FindEdgeEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindEdgeEdgeIntersections);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), FindEdgeEdgeGroup)

		EdgeEdgeIntersections = MakeShared<FEdgeEdgeIntersections>(
			GraphBuilder->Graph, UnionGraph, UnionFacade->Source, &EdgeEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingEdgeEdgeIntersections);

		FindEdgeEdgeGroup->OnCompleteCallback = [&]() { OnEdgeEdgeIntersectionsFound(); };
		FindEdgeEdgeGroup->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				if (!EdgeEdgeIntersections) { return; }
				const TSharedRef<FEdgeEdgeIntersections> EEI = EdgeEdgeIntersections.ToSharedRef();
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
				{
					const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[i];
					if (!Edge.bValid) { continue; }
					FindOverlappingEdges(EEI, i);
				}
			};
		FindEdgeEdgeGroup->PrepareRangesOnly(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::OnEdgeEdgeIntersectionsFound);

		if (!EdgeEdgeIntersections.Get()) { return; }
		if (!EdgeEdgeIntersections->InsertNodes())
		{
			OnEdgeEdgeIntersectionsComplete();
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		// Insert new nodes

		SortCrossingsGroup->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				if (!EdgeEdgeIntersections.Get()) { return; }

				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
				{
					FEdgeEdgeProxy& EdgeProxy = EdgeEdgeIntersections->Edges[i];
					const int32 IntersectionsNum = EdgeProxy.Intersections.Num();

					if (!IntersectionsNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&NewEdgesNum, (IntersectionsNum + 1));

					GraphBuilder->Graph->Edges[EdgeProxy.EdgeIndex].bValid = false; // Invalidate existing edge

					EdgeProxy.Intersections.Sort(
						[&](const int32& A, const int32& B)
						{
							return EdgeEdgeIntersections->Crossings[A].GetTime(EdgeProxy.EdgeIndex) > EdgeEdgeIntersections->Crossings[B].GetTime(EdgeProxy.EdgeIndex);
						});
				}
			};

		SortCrossingsGroup->OnCompleteCallback =
			[&]()
			{
				PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), BlendEdgeEdgeGroup)

				GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
				NewEdgesNum = 0;

				// TODO : Multi-thread by reserving the future edges in metadata and others.
				// Edge count is known and uniqueness is known in advance, we just need the number of edges, which we have.
				// Do set num instead of reserve, Graph has an InsertEdges that return the starting addition index.
				// use range prep to cache these and rebuild metadata and everything then.

				EdgeEdgeIntersections->InsertEdges(); // TODO : Async?
				UnionFacade->Source->CleanupKeys();

				if (bUseCustomEdgeEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomEdgeEdgeBlendingDetails); }
				else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(UnionFacade.ToSharedRef(), PCGExData::ESource::Out);

				BlendEdgeEdgeGroup->OnCompleteCallback = [&]() { OnEdgeEdgeIntersectionsComplete(); };
				BlendEdgeEdgeGroup->OnIterationRangeStartCallback =
					[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						if (!MetadataBlender) { return; }
						const TSharedRef<PCGExDataBlending::FMetadataBlender> Blender = MetadataBlender.ToSharedRef();

						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							EdgeEdgeIntersections->BlendIntersection(i, Blender);
						}
					};
				BlendEdgeEdgeGroup->PrepareRangesOnly(EdgeEdgeIntersections->Crossings.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
			};

		SortCrossingsGroup->PrepareRangesOnly(EdgeEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeIntersectionsComplete()
	{
		UnionFacade->Write(Context->GetAsyncManager());
	}

#pragma endregion

	void FUnionProcessor::WriteClusters()
	{
		Context->SetAsyncState(State_WritingClusters);
		GraphBuilder->OnCompilationEndCallback = [&](const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)
		{
			if (!bSuccess) { UnionFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput); }
			else { GraphBuilder->OutputEdgesToContext(); }
		};
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true, &GraphMetadataDetails);
	}
}

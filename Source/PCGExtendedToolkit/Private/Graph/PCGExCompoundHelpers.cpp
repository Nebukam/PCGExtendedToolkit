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
		const TSharedPtr<FCompoundGraph>& InCompoundGraph,
		const TSharedPtr<PCGExData::FFacade>& InCompoundFacade,
		const TArray<TSharedPtr<PCGExData::FFacade>>& InFacades,
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

		CompoundPointsBlender = MakeShared<PCGExDataBlending::FCompoundBlender>(&DefaultPointsBlendingDetails, InCarryOverDetails);

		TArray<FPCGPoint>& MutablePoints = CompoundFacade->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumCompoundNodes);

		CompoundPointsBlender->AddSources(InFacades);
		CompoundPointsBlender->PrepareMerge(CompoundFacade, CompoundGraph->PointsCompounds, nullptr); // TODO : Check if we want to ignore specific attributes // Answer : yes, cluster IDs etc

		Context->SetAsyncState(State_ProcessingCompound);

		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), ProcessNodesGroup)
		ProcessNodesGroup->OnCompleteCallback =
			[&]()
			{
				CompoundPointsBlender.Reset();

				CompoundFacade->Write(Context->GetAsyncManager());
				Context->SetAsyncState(PCGExMT::State_CompoundWriting);

				bRunning = true;

				GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
				GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
				GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);

				GraphBuilder = MakeShared<FGraphBuilder>(CompoundFacade, &InBuilderDetails, 4);

				TSet<uint64> UniqueEdges;
				CompoundGraph->GetUniqueEdges(UniqueEdges);
				CompoundGraph->WriteMetadata(GraphBuilder->Graph->NodeMetadata);

				GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);

				InternalStartExecution();
			};

		ProcessNodesGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			FCompoundNode* CompoundNode = CompoundGraph->Nodes[Index].Get();
			PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
			MutablePoints[Index] = CompoundNode->Point; // Copy "original" point properties, in case  there's only one

			FPCGPoint& Point = MutablePoints[Index];
			Point.MetadataEntry = Key; // Restore key

			Point.Transform.SetLocation(
				CompoundNode->UpdateCenter(CompoundGraph->PointsCompounds.Get(), Context->MainPoints.Get()));
			CompoundPointsBlender->MergeSingle(Index, PCGExDetails::GetDistanceDetails(PointPointIntersectionDetails));
		};

		ProcessNodesGroup->StartIterations(NumCompoundNodes, GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize, false, false);


		return true;
	}

	void FCompoundProcessor::InternalStartExecution()
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

	bool FCompoundProcessor::Execute()
	{
		if (!bRunning) { return false; }

		if (Context->IsState(State_ProcessingCompound)) { return false; }

		if (Context->IsState(State_ProcessingPointEdgeIntersections))
		{
			PCGEX_ASYNC_WAIT
			if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else { WriteClusters(); }
			return false;
		}

		if (Context->IsState(State_ProcessingEdgeEdgeIntersections))
		{
			PCGEX_ASYNC_WAIT
			WriteClusters();
			return false;
		}

		if (Context->IsState(State_WritingClusters))
		{
			PCGEX_ASYNC_WAIT
			return true;
		}

		return true;
	}

#pragma region PointEdge

	void FCompoundProcessor::FindPointEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindPointEdgeIntersections);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), FindPointEdgeGroup)

		PointEdgeIntersections = MakeShared<FPointEdgeIntersections>(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingPointEdgeIntersections);

		FindPointEdgeGroup->OnCompleteCallback = [&]() { FindPointEdgeIntersectionsFound(); };
		FindPointEdgeGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[Index];
			if (!Edge.bValid) { return; }
			FindCollinearNodes(PointEdgeIntersections.Get(), Index, CompoundFacade->Source->GetOut());
		};
		FindPointEdgeGroup->StartIterations(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FCompoundProcessor::FindPointEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindPointEdgeIntersectionsFound);

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
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomPointEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomPointEdgeBlendingDetails); }
				else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade.ToSharedRef(), PCGExData::ESource::Out);

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

	void FCompoundProcessor::OnPointEdgeIntersectionsComplete()
	{
		if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager()); }
	}

#pragma endregion

#pragma region EdgeEdge

	void FCompoundProcessor::FindEdgeEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::FindEdgeEdgeIntersections);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), FindEdgeEdgeGroup)

		EdgeEdgeIntersections = MakeShared<FEdgeEdgeIntersections>(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &EdgeEdgeIntersectionDetails);

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

	void FCompoundProcessor::OnEdgeEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundProcessor::OnEdgeEdgeIntersectionsFound);

		if (!EdgeEdgeIntersections.Get()) { return; }
		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		// Insert new nodes
		EdgeEdgeIntersections->InsertNodes(); // TODO : Async?

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
				CompoundFacade->Source->CleanupKeys();

				if (bUseCustomEdgeEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomEdgeEdgeBlendingDetails); }
				else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade.ToSharedRef(), PCGExData::ESource::Out);

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

	void FCompoundProcessor::OnEdgeEdgeIntersectionsComplete()
	{
		CompoundFacade->Write(Context->GetAsyncManager());
	}

#pragma endregion

	void FCompoundProcessor::WriteClusters()
	{
		Context->SetAsyncState(State_WritingClusters);
		GraphBuilder->OnCompilationEndCallback = [&](const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)
		{
			if (!bSuccess) { CompoundFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput); }
			else { GraphBuilder->OutputEdgesToContext(); }
		};
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true, &GraphMetadataDetails);
	}
}

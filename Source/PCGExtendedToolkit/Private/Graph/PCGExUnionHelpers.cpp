// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnionHelpers.h"

namespace PCGExGraph
{
	FUnionProcessor::FUnionProcessor(
		FPCGExPointsProcessorContext* InContext,
		TSharedRef<PCGExData::FFacade> InUnionDataFacade,
		TSharedRef<FUnionGraph> InUnionGraph,
		FPCGExPointPointIntersectionDetails InPointPointIntersectionSettings,
		FPCGExBlendingDetails InDefaultPointsBlending,
		FPCGExBlendingDetails InDefaultEdgesBlending):
		Context(InContext),
		UnionDataFacade(InUnionDataFacade),
		UnionGraph(InUnionGraph),
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
		const TArray<TSharedRef<PCGExData::FFacade>>& InFacades,
		const FPCGExGraphBuilderDetails& InBuilderDetails)
	{
		BuilderDetails = InBuilderDetails;

		const int32 NumUnionNodes = UnionGraph->Nodes.Num();
		if (NumUnionNodes == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Union graph is empty. Something is likely corrupted."));
			return false;
		}

		Context->SetAsyncState(State_ProcessingUnion);

		UnionPointsBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&DefaultPointsBlendingDetails, VtxCarryOverDetails);

		TArray<FPCGPoint>& MutablePoints = UnionDataFacade->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumUnionNodes);

		UnionPointsBlender->AddSources(InFacades, &ProtectedClusterAttributes);
		UnionPointsBlender->PrepareMerge(Context, UnionDataFacade, UnionGraph->NodesUnion);

		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), ProcessNodesGroup)

		ProcessNodesGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnNodesProcessingComplete();
			};

		ProcessNodesGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				const TSharedPtr<PCGExData::FUnionMetadata> PointsUnion = This->UnionGraph->NodesUnion;
				const TSharedPtr<PCGExData::FPointIOCollection> MainPoints = This->Context->MainPoints;
				const TSharedPtr<PCGExDataBlending::FUnionBlender> Blender = This->UnionPointsBlender;

				TSharedPtr<PCGExDetails::FDistances> Distances = PCGExDetails::MakeDistances(This->PointPointIntersectionDetails.FuseDetails.SourceDistance, This->PointPointIntersectionDetails.FuseDetails.TargetDistance);

				TArray<FPCGPoint>& Points = This->UnionDataFacade->GetOut()->GetMutablePoints();

				for (int i = Scope.Start; i < Scope.End; i++)
				{
					TSharedPtr<FUnionNode> UnionNode = This->UnionGraph->Nodes[i];
					const PCGMetadataEntryKey Key = Points[i].MetadataEntry;
					Points[i] = UnionNode->Point; // Copy "original" point properties, in case  there's only one

					FPCGPoint& Point = Points[i];
					Point.MetadataEntry = Key; // Restore key

					Point.Transform.SetLocation(UnionNode->UpdateCenter(PointsUnion, MainPoints));
					Blender->MergeSingle(i, Distances);
				}
			};

		ProcessNodesGroup->StartSubLoops(NumUnionNodes, GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize, false);


		return true;
	}

	void FUnionProcessor::OnNodesProcessingComplete()
	{
		UnionPointsBlender.Reset();

		bRunning = true;

		GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
		GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
		GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);
		GraphMetadataDetails.EdgesBlendingDetailsPtr = bUseCustomEdgeEdgeBlending ? &CustomEdgeEdgeBlendingDetails : &DefaultEdgesBlendingDetails;
		GraphMetadataDetails.EdgesCarryOverDetails = EdgesCarryOverDetails;

		GraphBuilder = MakeShared<FGraphBuilder>(UnionDataFacade, &BuilderDetails, 4);
		GraphBuilder->SourceEdgeFacades = SourceEdgesIO;
		GraphBuilder->Graph->NodesUnion = UnionGraph->NodesUnion;
		GraphBuilder->Graph->EdgesUnion = UnionGraph->EdgesUnion;

		TArray<FEdge> UniqueEdges;
		UnionGraph->GetUniqueEdges(UniqueEdges);
		GraphBuilder->Graph->InsertEdges(UniqueEdges);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), WriteMetadataTask);
		WriteMetadataTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->UnionDataFacade->Flush();
				This->InternalStartExecution();
			};

		UnionDataFacade->WriteBuffersAsCallbacks(WriteMetadataTask);

		WriteMetadataTask->AddSimpleCallback(
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->UnionGraph->WriteNodeMetadata(This->GraphBuilder->Graph);
			});

		WriteMetadataTask->AddSimpleCallback(
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->UnionGraph->WriteEdgeMetadata(This->GraphBuilder->Graph);
			});

		WriteMetadataTask->StartSimpleCallbacks();
	}

	void FUnionProcessor::InternalStartExecution()
	{
		if (GraphBuilder->Graph->Edges.Num() <= 1)
		{
			CompileFinalGraph(); // Nothing to be found
		}
		else if (bDoPointEdge)
		{
			FindPointEdgeIntersections();
		}
		else if (bDoEdgeEdge)
		{
			FindEdgeEdgeIntersections();
		}
		else
		{
			CompileFinalGraph();
		}
	}

	bool FUnionProcessor::Execute()
	{
		if (!bRunning || Context->IsState(State_ProcessingUnion)) { return false; }

		PCGEX_ON_ASYNC_STATE_READY(State_ProcessingPointEdgeIntersections)
		{
			if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else
			{
				CompileFinalGraph();
			}
			return false;
		}

		PCGEX_ON_ASYNC_STATE_READY(State_ProcessingEdgeEdgeIntersections)
		{
			CompileFinalGraph();
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
			GraphBuilder->Graph, UnionDataFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingPointEdgeIntersections);

		FindPointEdgeGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->FindPointEdgeIntersectionsFound();
			};
		FindPointEdgeGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					const FEdge& Edge = This->GraphBuilder->Graph->Edges[i];
					if (!Edge.bValid) { continue; }
					FindCollinearNodes(This->PointEdgeIntersections, i, This->UnionDataFacade->Source->GetOut());
				}
			};
		FindPointEdgeGroup->StartSubLoops(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::FindPointEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindPointEdgeIntersectionsFound);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		SortCrossingsGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					FPointEdgeProxy& PointEdgeProxy = This->PointEdgeIntersections->Edges[i];
					const int32 CollinearNum = PointEdgeProxy.CollinearPoints.Num();

					if (!CollinearNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&This->NewEdgesNum, (CollinearNum + 1));

					FEdge& SplitEdge = This->GraphBuilder->Graph->Edges[PointEdgeProxy.EdgeIndex];
					FPlatformAtomics::InterlockedExchange(&SplitEdge.bValid, 0); // Invalidate existing edge
					PointEdgeProxy.CollinearPoints.Sort([](const FPESplit& A, const FPESplit& B) { return A.Time < B.Time; });
				}
			};

		SortCrossingsGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnPointEdgeSortingComplete();
			};

		SortCrossingsGroup->StartSubLoops(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		///
	}

	void FUnionProcessor::OnPointEdgeSortingComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), BlendPointEdgeGroup)

		GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
		NewEdgesNum = 0;

		PointEdgeIntersections->Insert(); // TODO : Async?
		UnionDataFacade->Source->CleanupKeys();

		if (bUseCustomPointEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomPointEdgeBlendingDetails); }
		else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

		MetadataBlender->PrepareForData(UnionDataFacade, PCGExData::ESource::Out, true, &ProtectedClusterAttributes);

		BlendPointEdgeGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnPointEdgeIntersectionsComplete();
			};

		BlendPointEdgeGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				if (!This->MetadataBlender) { return; }
				const TSharedRef<PCGExDataBlending::FMetadataBlender> Blender = This->MetadataBlender.ToSharedRef();

				for (int i = Scope.Start; i < Scope.End; i++)
				{
					// TODO
				}
			};
		BlendPointEdgeGroup->StartSubLoops(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnPointEdgeIntersectionsComplete() const
	{
		if (MetadataBlender) { UnionDataFacade->Write(Context->GetAsyncManager()); }
	}

#pragma endregion

#pragma region EdgeEdge

	void FUnionProcessor::FindEdgeEdgeIntersections()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindEdgeEdgeIntersections);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), FindEdgeEdgeGroup)

		EdgeEdgeIntersections = MakeShared<FEdgeEdgeIntersections>(
			GraphBuilder->Graph, UnionGraph, UnionDataFacade->Source, &EdgeEdgeIntersectionDetails);

		Context->SetAsyncState(State_ProcessingEdgeEdgeIntersections);

		FindEdgeEdgeGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnEdgeEdgeIntersectionsFound();
			};

		FindEdgeEdgeGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				if (!This->EdgeEdgeIntersections) { return; }
				const TSharedRef<FEdgeEdgeIntersections> EEI = This->EdgeEdgeIntersections.ToSharedRef();

				for (int i = Scope.Start; i < Scope.End; i++)
				{
					const FEdge& Edge = This->GraphBuilder->Graph->Edges[i];
					if (!Edge.bValid) { continue; }
					FindOverlappingEdges(EEI, i);
				}
			};


		FindEdgeEdgeGroup->StartSubLoops(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::OnEdgeEdgeIntersectionsFound);

		if (!EdgeEdgeIntersections) { return; }
		if (!EdgeEdgeIntersections->InsertNodes())
		{
			OnEdgeEdgeIntersectionsComplete();
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		// Insert new nodes
		SortCrossingsGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				if (!This->EdgeEdgeIntersections) { return; }
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					FEdgeEdgeProxy& EdgeProxy = This->EdgeEdgeIntersections->Edges[i];
					const int32 IntersectionsNum = EdgeProxy.Intersections.Num();

					if (!IntersectionsNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&This->NewEdgesNum, (IntersectionsNum + 1));

					This->GraphBuilder->Graph->Edges[EdgeProxy.EdgeIndex].bValid = false; // Invalidate existing edge

					EdgeProxy.Intersections.Sort(
						[&This, &EdgeProxy](const int32& A, const int32& B)
						{
							return This->EdgeEdgeIntersections->Crossings[A].GetTime(EdgeProxy.EdgeIndex) < This->EdgeEdgeIntersections->Crossings[B].GetTime(EdgeProxy.EdgeIndex);
						});
				}
			};

		SortCrossingsGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnEdgeEdgeSortingComplete();
			};

		SortCrossingsGroup->StartSubLoops(EdgeEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeSortingComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), BlendEdgeEdgeGroup)

		GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
		NewEdgesNum = 0;

		// TODO : Multi-thread by reserving the future edges in metadata and others.
		// Edge count is known and uniqueness is known in advance, we just need the number of edges, which we have.
		// Do set num instead of reserve, Graph has an InsertEdges that return the starting addition index.
		// use range prep to cache these and rebuild metadata and everything then.

		EdgeEdgeIntersections->InsertEdges(); // TODO : Async?
		UnionDataFacade->Source->CleanupKeys();

		if (bUseCustomEdgeEdgeBlending) { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&CustomEdgeEdgeBlendingDetails); }
		else { MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&DefaultPointsBlendingDetails); }

		MetadataBlender->PrepareForData(UnionDataFacade, PCGExData::ESource::Out, true, &ProtectedClusterAttributes);

		BlendEdgeEdgeGroup->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->OnEdgeEdgeIntersectionsComplete();
		};

		BlendEdgeEdgeGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				if (!This->MetadataBlender) { return; }
				const TSharedRef<PCGExDataBlending::FMetadataBlender> Blender = This->MetadataBlender.ToSharedRef();

				for (int i = Scope.Start; i < Scope.End; i++) { This->EdgeEdgeIntersections->BlendIntersection(i, Blender); }
			};
		BlendEdgeEdgeGroup->StartSubLoops(EdgeEdgeIntersections->Crossings.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeIntersectionsComplete() const
	{
		UnionDataFacade->Write(Context->GetAsyncManager());
	}

#pragma endregion

	void FUnionProcessor::CompileFinalGraph()
	{
		check(!bCompilingFinalGraph)

		bCompilingFinalGraph = true;

		Context->SetAsyncState(State_WritingClusters);
		GraphBuilder->OnCompilationEndCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)
			{
				PCGEX_ASYNC_THIS
				if (!bSuccess) { This->UnionDataFacade->Source->InitializeOutput(PCGExData::EIOInit::None); }
				else { This->GraphBuilder->StageEdgesOutputs(); }
			};
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true, &GraphMetadataDetails);
	}
}

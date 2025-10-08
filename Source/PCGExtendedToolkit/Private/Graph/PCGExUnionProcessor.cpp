﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnionProcessor.h"

#include "PCGExHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsDistances.h"

namespace PCGExGraph
{
	FUnionProcessor::FUnionProcessor(
		FPCGExPointsProcessorContext* InContext,
		TSharedRef<PCGExData::FFacade> InUnionDataFacade,
		TSharedRef<FUnionGraph> InUnionGraph,
		FPCGExPointPointIntersectionDetails InPointPointIntersectionSettings,
		FPCGExBlendingDetails InDefaultPointsBlending,
		FPCGExBlendingDetails InDefaultEdgesBlending)
		: Context(InContext),
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
		EdgeEdgeIntersectionDetails.Init();
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

		Distances = PCGExDetails::MakeDistances(PointPointIntersectionDetails.FuseDetails.SourceDistance, PointPointIntersectionDetails.FuseDetails.TargetDistance);


		const TSharedPtr<PCGExDataBlending::FUnionBlender> TypedBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&DefaultPointsBlendingDetails, VtxCarryOverDetails, Distances);
		UnionBlender = TypedBlender;

		TypedBlender->AddSources(InFacades, &ProtectedClusterAttributes);

		UPCGBasePointData* MutablePoints = UnionDataFacade->GetOut();
		PCGEx::SetNumPointsAllocated(MutablePoints, NumUnionNodes, UnionBlender->GetAllocatedProperties()); // TODO : Proper Allocation

		if (!TypedBlender->Init(Context, UnionDataFacade, UnionGraph->NodesUnion)) { return false; }

		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), ProcessNodesGroup)

		ProcessNodesGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->UnionBlender.Reset();
				This->OnNodesProcessingComplete();
			};

		ProcessNodesGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				const TSharedPtr<PCGExData::FUnionMetadata> PointsUnion = This->UnionGraph->NodesUnion;
				const TSharedPtr<PCGExData::FPointIOCollection> MainPoints = This->Context->MainPoints;
				const TSharedPtr<PCGExDataBlending::IUnionBlender> Blender = This->UnionBlender;

				TArray<PCGExData::FWeightedPoint> WeightedPoints;
				TArray<PCGEx::FOpStats> Trackers;
				Blender->InitTrackers(Trackers);

				UPCGBasePointData* OutPoints = This->UnionDataFacade->GetOut();
				TPCGValueRange<FTransform> OutTransforms = OutPoints->GetTransformValueRange(false);

				PCGEX_SCOPE_LOOP(Index)
				{
					TSharedPtr<FUnionNode> UnionNode = This->UnionGraph->Nodes[Index];

					//const PCGMetadataEntryKey Key = OutPoints[i].MetadataEntry;
					//OutPoints[Index] = UnionNode->Point; // Copy "original" point properties, in case  there's only one

					//FPCGPoint& Point = OutPoints[Index];
					//Point.MetadataEntry = Key; // Restore key

					OutTransforms[Index].SetLocation(UnionNode->UpdateCenter(PointsUnion, MainPoints));
					Blender->MergeSingle(Index, WeightedPoints, Trackers);
				}
			};

		ProcessNodesGroup->StartSubLoops(NumUnionNodes, GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize, false);


		return true;
	}

	void FUnionProcessor::OnNodesProcessingComplete()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::OnNodesProcessingComplete);

		UnionBlender.Reset();

		bRunning = true;

		GraphMetadataDetails.Grab(Context, PointPointIntersectionDetails);
		GraphMetadataDetails.Grab(Context, PointEdgeIntersectionDetails);
		GraphMetadataDetails.Grab(Context, EdgeEdgeIntersectionDetails);
		GraphMetadataDetails.EdgesBlendingDetailsPtr = bUseCustomEdgeEdgeBlending ? &CustomEdgeEdgeBlendingDetails : &DefaultEdgesBlendingDetails;
		GraphMetadataDetails.EdgesCarryOverDetails = EdgesCarryOverDetails;

		GraphBuilder = MakeShared<FGraphBuilder>(UnionDataFacade, &BuilderDetails);
		GraphBuilder->bInheritNodeData = false;
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
				PCGEX_SCOPE_LOOP(Index)
				{
					const FEdge& Edge = This->GraphBuilder->Graph->Edges[Index];
					if (!Edge.bValid) { continue; }
					FindCollinearNodes(This->PointEdgeIntersections, Index, This->UnionDataFacade->Source->GetOut());
				}
			};
		FindPointEdgeGroup->StartSubLoops(GraphBuilder->Graph->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::FindPointEdgeIntersectionsFound()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionProcessor::FindPointEdgeIntersectionsFound);

		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), SortCrossingsGroup)

		SortCrossingsGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnPointEdgeSortingComplete();
			};

		SortCrossingsGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				PCGEX_SCOPE_LOOP(Index)
				{
					FPointEdgeProxy& PointEdgeProxy = This->PointEdgeIntersections->Edges[Index];
					const int32 CollinearNum = PointEdgeProxy.CollinearPoints.Num();

					if (!CollinearNum) { continue; }

					FPlatformAtomics::InterlockedAdd(&This->NewEdgesNum, (CollinearNum + 1));

					FEdge& SplitEdge = This->GraphBuilder->Graph->Edges[PointEdgeProxy.EdgeIndex];
					FPlatformAtomics::InterlockedExchange(&SplitEdge.bValid, 0); // Invalidate existing edge
					PointEdgeProxy.CollinearPoints.Sort([](const FPESplit& A, const FPESplit& B) { return A.Time < B.Time; });
				}
			};

		SortCrossingsGroup->StartSubLoops(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		///
	}

	void FUnionProcessor::OnPointEdgeSortingComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(Context->GetAsyncManager(), BlendPointEdgeGroup)

		GraphBuilder->Graph->ReserveForEdges(NewEdgesNum);
		NewEdgesNum = 0;

		PointEdgeIntersections->Insert();
		UnionDataFacade->Source->ClearCachedKeys();

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
		MetadataBlender->SetTargetData(UnionDataFacade);
		MetadataBlender->SetSourceData(UnionDataFacade, PCGExData::EIOSide::Out);

		if (!MetadataBlender->Init(Context, bUseCustomPointEdgeBlending ? CustomPointEdgeBlendingDetails : DefaultPointsBlendingDetails, &ProtectedClusterAttributes))
		{
			// Fail
			Context->CancelExecution(FString("Error initializing Point/Edge blending"));
			return;
		}

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

				PCGEX_SCOPE_LOOP(Index)
				{
					// TODO
				}
			};

		BlendPointEdgeGroup->StartSubLoops(PointEdgeIntersections->Edges.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnPointEdgeIntersectionsComplete() const
	{
		if (MetadataBlender) { UnionDataFacade->WriteFastest(Context->GetAsyncManager()); }
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

				PCGEX_SCOPE_LOOP(Index)
				{
					const FEdge& Edge = This->GraphBuilder->Graph->Edges[Index];
					if (!Edge.bValid) { continue; }
					FindOverlappingEdges(EEI, Index);
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
				PCGEX_SCOPE_LOOP(Index)
				{
					FEdgeEdgeProxy& EdgeProxy = This->EdgeEdgeIntersections->Edges[Index];
					const int32 NumIntersections = EdgeProxy.Intersections.Num();

					if (!NumIntersections) { continue; }

					FPlatformAtomics::InterlockedAdd(&This->NewEdgesNum, (NumIntersections + 1));

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
		UnionDataFacade->Source->ClearCachedKeys();

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
		MetadataBlender->SetTargetData(UnionDataFacade);
		MetadataBlender->SetSourceData(UnionDataFacade, PCGExData::EIOSide::Out);

		if (!MetadataBlender->Init(Context, bUseCustomEdgeEdgeBlending ? CustomEdgeEdgeBlendingDetails : DefaultPointsBlendingDetails, &ProtectedClusterAttributes))
		{
			// Fail
			Context->CancelExecution(FString("Error initializing Edge/Edge blending"));
			return;
		}

		BlendEdgeEdgeGroup->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->OnEdgeEdgeIntersectionsComplete();
		};

		BlendEdgeEdgeGroup->OnPrepareSubLoopsCallback = [PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops)
		{
			PCGEX_ASYNC_THIS
		};

		BlendEdgeEdgeGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				if (!This->MetadataBlender) { return; }
				const TSharedRef<PCGExDataBlending::FMetadataBlender> Blender = This->MetadataBlender.ToSharedRef();

				TArray<PCGEx::FOpStats> Trackers;
				Blender->InitTrackers(Trackers);

				PCGEX_SCOPE_LOOP(Index) { This->EdgeEdgeIntersections->BlendIntersection(Index, Blender, Trackers); }
			};

		BlendEdgeEdgeGroup->StartSubLoops(EdgeEdgeIntersections->Crossings.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
	}

	void FUnionProcessor::OnEdgeEdgeIntersectionsComplete() const
	{
		UnionDataFacade->WriteFastest(Context->GetAsyncManager());
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
				if (!bSuccess) { This->UnionDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoInit); }
				else { This->GraphBuilder->StageEdgesOutputs(); }
			};

		// Make sure we provide up-to-date transform range to sort over
		GraphBuilder->NodePointsTransforms = GraphBuilder->NodeDataFacade->GetOut()->GetConstTransformValueRange();
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true, &GraphMetadataDetails);
	}
}

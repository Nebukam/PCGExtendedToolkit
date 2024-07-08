// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExCompoundHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExPathToClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties>
UPCGExPathToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(
		PCGExGraph::OutputEdgesLabel,
		"Point data representing edges.",
		Required,
		{})
	return PinProperties;
}

PCGExData::EInit UPCGExPathToClustersSettings::GetMainOutputInitMode() const
{
	return PCGExData::EInit::NoOutput;
}

PCGEX_INITIALIZE_ELEMENT(PathToClusters)

FPCGExPathToClustersContext::~FPCGExPathToClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(PathsFacades)

	if (CompoundFacade)
	{
		PCGEX_DELETE(CompoundFacade->Source)
		PCGEX_DELETE(CompoundFacade)
	}

	PCGEX_DELETE(CompoundProcessor)
}

bool FPCGExPathToClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	const_cast<UPCGExPathToClustersSettings*>(Settings)
		->EdgeEdgeIntersectionDetails.ComputeDot();

	Context->CompoundProcessor = new PCGExGraph::FCompoundProcessor(
		Context,
		Settings->PointPointIntersectionDetails,
		Settings->DefaultPointsBlendingDetails,
		Settings->DefaultEdgesBlendingDetails);

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->CompoundProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingDetails);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->CompoundProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingDetails);
	}

	if (Settings->bFusePaths)
	{
		PCGExData::FPointIO* CompoundPoints = new PCGExData::FPointIO(nullptr);
		CompoundPoints->SetInfos(-1, Settings->GetMainOutputLabel());
		CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(
			PCGExData::EInit::NewOutput);

		Context->CompoundFacade = new PCGExData::FFacade(CompoundPoints);

		Context->CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionDetails.FuseDetails,
			Context->MainPoints->GetInBounds().ExpandBy(10),
			true,
			Settings->PointPointIntersectionDetails.FuseMethod);

		Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(
			&Settings->DefaultPointsBlendingDetails, &Context->CarryOverDetails);
	}

	return true;
}

bool FPCGExPathToClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->bFusePaths)
		{
			if (!Context->StartBatchProcessingPoints<
				PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>>(
				[](const PCGExData::FPointIO* Entry)
				{
					return Entry->GetNum() >= 2;
				},
				[&](PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* NewBatch)
				{
					NewBatch->bInlineProcessing = true;
				},
				PCGExGraph::State_ProcessingCompound))
			{
				PCGE_LOG(
					Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
		else
		{
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<
				PCGExPathToClusters::FNonFusingProcessor>>(
				[](const PCGExData::FPointIO* Entry)
				{
					return Entry->GetNum() >= 2;
				},
				[&](PCGExPointsMT::TBatch<
				PCGExPathToClusters::FNonFusingProcessor>* NewBatch)
				{
				},
				PCGExMT::State_Done))
			{
				PCGE_LOG(
					Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
	}

	if (!Context->ProcessPointsBatch())
	{
		return false;
	}

#pragma region Intersection management

	if (Settings->bFusePaths)
	{
		if (Context->IsState(PCGExGraph::State_ProcessingCompound))
		{
			const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
			if (NumCompoundNodes == 0)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Compound graph is empty. Vtx/Edge pairs are likely corrupted."));
				return true;
			}

			TArray<FPCGPoint>& MutablePoints = Context->CompoundFacade->GetOut()->GetMutablePoints();

			auto Initialize = [&]()
			{
				Context->PathsFacades.Reserve(Context->MainBatch->ProcessorFacades.Num());
				Context->PathsFacades.Append(Context->MainBatch->ProcessorFacades);

				PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* MainBatch = static_cast<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>*>(Context->MainBatch);
				for (PCGExPathToClusters::FFusingProcessor* Processor : MainBatch->Processors)
				{
					Processor->PointDataFacade = nullptr; // Remove ownership of facade
					// before deleting the processor
				}

				PCGEX_DELETE(Context->MainBatch);

				Context->CompoundFacade->Source->InitializeNum(
					NumCompoundNodes, true);
				Context->CompoundPointsBlender->AddSources(Context->PathsFacades);
				Context->CompoundPointsBlender->PrepareMerge(
					Context->CompoundFacade, Context->CompoundGraph->PointsCompounds);
			};

			auto ProcessNode = [&](const int32 Index)
			{
				PCGExGraph::FCompoundNode* CompoundNode =
					Context->CompoundGraph->Nodes[Index];
				PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
				MutablePoints[Index] =
					CompoundNode->Point; // Copy "original" point properties, in case
				// there's only one

				FPCGPoint& Point = MutablePoints[Index];
				Point.MetadataEntry = Key; // Restore key

				Point.Transform.SetLocation(
					CompoundNode->UpdateCenter(
						Context->CompoundGraph->PointsCompounds, Context->MainPoints));
				Context->CompoundPointsBlender->MergeSingle(
					Index,
					PCGExDetails::GetDistanceDetails(
						Settings->PointPointIntersectionDetails));
			};

			if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes))
			{
				return false;
			}

			PCGEX_DELETE(Context->CompoundPointsBlender)

			Context->CompoundFacade->Write(Context->GetAsyncManager(), true);
			Context->SetAsyncState(PCGExMT::State_CompoundWriting);
		}

		if (Context->IsState(PCGExMT::State_CompoundWriting))
		{
			PCGEX_ASYNC_WAIT

			Context->CompoundProcessor->StartProcessing(
				Context->CompoundGraph,
				Context->CompoundFacade,
				Settings->GraphBuilderDetails,
				[&](PCGExGraph::FGraphBuilder* GraphBuilder)
				{
					TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
					Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
					Context->CompoundGraph->WriteMetadata(
						GraphBuilder->Graph->NodeMetadata);

					GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
				});
		}

		if (!Context->CompoundProcessor->Execute()) { return false; }

		Context->Done();
	}

#pragma endregion

	if (Context->IsDone())
	{
		if (Settings->bFusePaths) { Context->CompoundFacade->Source->OutputTo(Context); }
		else { Context->OutputMainPoints(); }
	}

	return Context->TryComplete();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	FNonFusingProcessor::~FNonFusingProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool FNonFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager))
		{
			return false;
		}

		GraphBuilder = new PCGExGraph::FGraphBuilder(
			PointIO, &Settings->GraphBuilderDetails, 2);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();

		PointIO->InitializeOutput<UPCGExClusterNodesData>(
			PCGExData::EInit::NewOutput);

		TArray<PCGExGraph::FIndexedEdge> Edges;

		Edges.SetNumUninitialized(
			Settings->bClosedPath ? NumPoints : NumPoints - 1);

		for (int i = 0; i < Edges.Num(); i++)
		{
			Edges[i] = PCGExGraph::FIndexedEdge(i, i, i + 1, PointIO->IOIndex);
		}

		if (Settings->bClosedPath)
		{
			const int32 LastIndex = Edges.Num() - 1;
			Edges[LastIndex] =
				PCGExGraph::FIndexedEdge(LastIndex, LastIndex, 0, PointIO->IOIndex);
		}

		GraphBuilder->Graph->InsertEdges(Edges);
		Edges.Empty();

		GraphBuilder->CompileAsync(AsyncManagerPtr);

		return true;
	}

	void FNonFusingProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->Write(Context);
	}

#pragma endregion

#pragma region Fusing

	FFusingProcessor::~FFusingProcessor()
	{
	}

	bool FFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		const int32 IOIndex = PointIO->IOIndex;

		if (NumPoints < 2) { return false; }

		CompoundGraph = TypedContext->CompoundGraph;

		for (int i = 0; i < NumPoints; i++)
		{
			PCGExGraph::FCompoundNode* CurrentVtx = CompoundGraph->InsertPointUnsafe(InPoints[i], IOIndex, i);

			if (const int32 PrevIndex = i - 1; InPoints.IsValidIndex(PrevIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx = CompoundGraph->InsertPoint(InPoints[PrevIndex], IOIndex, PrevIndex);

				CurrentVtx->Adjacency.Add(OtherVtx->Index);
				OtherVtx->Adjacency.Add(CurrentVtx->Index);
			}

			if (const int32 NextIndex = i + 1; InPoints.IsValidIndex(NextIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx = CompoundGraph->InsertPoint(InPoints[NextIndex], IOIndex, NextIndex);

				CurrentVtx->Adjacency.Add(OtherVtx->Index);
				OtherVtx->Adjacency.Add(CurrentVtx->Index);
			}
		}

		if (Settings->bClosedPath)
		{
			// Create an edge between first and last points
			const int32 LastIndex = NumPoints - 1;
			CompoundGraph->InsertEdge(InPoints[0], IOIndex, 0, InPoints[LastIndex], IOIndex, LastIndex);
		}

		return true;
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

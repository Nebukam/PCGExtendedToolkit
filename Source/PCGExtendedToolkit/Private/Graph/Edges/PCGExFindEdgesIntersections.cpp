// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExFindEdgesIntersections.h"

#include "Data/PCGExGraphParamsData.h"
#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

namespace PCGExGraph
{
	void FEdgeCrossingsHandler::Prepare(const TArray<FPCGPoint>& InPoints)
	{
		for (int i = 0; i < NumEdges; i++)
		{
			const FIndexedEdge& Edge = Graph->Edges[i];
			FBox& NewBox = SegmentBounds.Emplace_GetRef(ForceInit);
			NewBox += InPoints[Edge.Start].Transform.GetLocation();
			NewBox += InPoints[Edge.End].Transform.GetLocation();
		}
	}

	void FEdgeCrossingsHandler::ProcessEdge(const int32 EdgeIndex, const TArray<FPCGPoint>& InPoints)

	{
		TArray<FIndexedEdge>& Edges = Graph->Edges;

		const FIndexedEdge& Edge = Edges[EdgeIndex];
		const FBox CurrentBox = SegmentBounds[EdgeIndex].ExpandBy(Tolerance);
		const FVector A1 = InPoints[Edge.Start].Transform.GetLocation();
		const FVector B1 = InPoints[Edge.End].Transform.GetLocation();

		for (int i = 0; i < NumEdges; i++)
		{
			if (CurrentBox.Intersect(SegmentBounds[i]))
			{
				const FIndexedEdge& OtherEdge = Edges[i];
				FVector A2 = InPoints[OtherEdge.Start].Transform.GetLocation();
				FVector B2 = InPoints[OtherEdge.End].Transform.GetLocation();
				FVector A3;
				FVector B3;
				FMath::SegmentDistToSegment(A1, B1, A2, B2, A3, B3);
				const bool bIsEnd = A1 == A3 || B1 == A3 || A2 == A3 || B2 == A3 || A1 == B3 || B1 == B3 || A2 == B3 || B2 == B3;
				if (!bIsEnd && FVector::DistSquared(A3, B3) < SquaredTolerance)
				{
					FWriteScopeLock WriteLock(CrossingLock);
					FEdgeCrossing& EdgeCrossing = Crossings.Emplace_GetRef();
					EdgeCrossing.EdgeA = EdgeIndex;
					EdgeCrossing.EdgeB = i;
					EdgeCrossing.Center = FMath::Lerp(A3, B3, 0.5);
				}
			}
		}
	}

	void FEdgeCrossingsHandler::InsertCrossings()
	{
		TArray<FNode>& Nodes = Graph->Nodes;
		TArray<FIndexedEdge>& Edges = Graph->Edges;

		Nodes.Reserve(Nodes.Num() + Crossings.Num());
		if (!Crossings.IsEmpty()) { Graph->bRequiresConsolidation = true; }

		FIndexedEdge NewEdge = FIndexedEdge{};

		for (const FEdgeCrossing& EdgeCrossing : Crossings)
		{
			Edges[EdgeCrossing.EdgeA].bValid = false;
			Edges[EdgeCrossing.EdgeB].bValid = false;

			FNode& NewNode = Nodes.Emplace_GetRef();
			NewNode.NodeIndex = Nodes.Num() - 1;
			NewNode.Edges.Reserve(4);

			const FIndexedEdge& EdgeA = Edges[EdgeCrossing.EdgeA];
			const FIndexedEdge& EdgeB = Edges[EdgeCrossing.EdgeB];

			Graph->InsertEdge(NewNode.NodeIndex, EdgeA.Start, NewEdge);
			Graph->InsertEdge(NewNode.NodeIndex, EdgeA.End, NewEdge);
			Graph->InsertEdge(NewNode.NodeIndex, EdgeB.Start, NewEdge);
			Graph->InsertEdge(NewNode.NodeIndex, EdgeB.End, NewEdge);
		}
	}
}

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFindEdgesIntersectionsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExFindEdgesIntersectionsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFindEdgesIntersectionsContext::~FPCGExFindEdgesIntersectionsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
}

PCGEX_INITIALIZE_ELEMENT(FindEdgesIntersections)

bool FPCGExFindEdgesIntersectionsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgesIntersections)
	PCGEX_FWD(CrossingTolerance)

	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

bool FPCGExFindEdgesIntersectionsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgesIntersectionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgesIntersections)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();

		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->CurrentEdges);

		//Context->GraphBuilder->Graph->InsertEdges(Context->IndexedEdges);

		//TODO: Insert merged edges BEFORE compilation 

		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

bool FPCGExFindIntersectionsTask::ExecuteTask()
{
	/*
	for (int i = 0; i < Builder->EdgeCrossings->Crossings.Num(); i++)
			{
				if (!Builder->Graph->Nodes[Builder->EdgeCrossings->StartIndex + i].bValid) { continue; }
				MutablePoints.Last().Transform.SetLocation(Builder->EdgeCrossings->Crossings[i].Center);
			}

	if (Builder->EdgeCrossings)
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
			for (const PCGExGraph::FEdgeCrossing& Crossing : Builder->EdgeCrossings->Crossings)
			{
				FPCGPoint& NewPoint = MutablePoints.Emplace_GetRef();
				NewPoint.Transform.SetLocation(Crossing.Center);
				PCGEx::RandomizeSeed(NewPoint);
			}
		}
	 */

	return false;
}

#undef LOCTEXT_NAMESPACE

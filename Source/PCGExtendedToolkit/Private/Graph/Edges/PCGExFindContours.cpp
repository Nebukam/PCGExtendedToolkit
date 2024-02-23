// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExFindContours.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExFindContours"
#define PCGEX_NAMESPACE FindContours

UPCGExFindContoursSettings::UPCGExFindContoursSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExFindContoursSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Seeds associated with the main input points");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindContoursSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Contours");
#endif

	return PinProperties;
}

PCGExData::EInit UPCGExFindContoursSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFindContoursSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(FindContours)

FPCGExFindContoursContext::~FPCGExFindContoursContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Seeds)
	PCGEX_DELETE(Paths)

	for (TArray<FVector>* Array : ProjectedSeeds)
	{
		Array->Empty();
		delete Array;
	}

	ProjectedSeeds.Empty();
}


bool FPCGExFindContoursElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)
	
	Context->Seeds = new PCGExData::FPointIOCollection(Context, PCGExPathfinding::SourceSeedsLabel);
	if (Context->Seeds->Pairs.IsEmpty()) { return false; }
	
	for (const PCGExData::FPointIO* SeedIO : Context->Seeds->Pairs)
	{
		TArray<FVector>* SeedsArray = new TArray<FVector>();
		Context->ProjectedSeeds.Add(SeedsArray);
		
		Settings->ProjectionSettings.Project(SeedIO->GetIn()->GetPoints(), SeedsArray);
	}

	Context->Paths = new PCGExData::FPointIOCollection();
	Context->Paths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExFindContoursElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindContoursElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

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
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; } // Corrupted or invalid cluster

		Context->CurrentCluster->ProjectNodes(Settings->ProjectionSettings);

		for (int i = 0; i < Context->Seeds->Pairs.Num(); i++)
		{
			Context->GetAsyncManager()->Start<FPCGExFindContourTask>(i, &Context->Paths->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput), Context->CurrentCluster);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->Paths->OutputTo(Context);
	}

	return Context->IsDone();
}

bool FPCGExFindContourTask::ExecuteTask()
{
	FPCGExFindContoursContext* Context = static_cast<FPCGExFindContoursContext*>(Manager->Context);
	PCGEX_SETTINGS(FindContours)

	// Current PointIO is the output path.
	const TArray<FVector>& Guides = *Context->ProjectedSeeds[TaskIndex];

	if (Guides.IsEmpty())
	{
		// Guides are empty. Should never happen as we filter IOs
		return false;
	}

	const FVector Guide = Guides[0];
	const int32 StartNodeIndex = Cluster->FindClosestNode(Guide, 2);

	if (StartNodeIndex == -1)
	{
		// Fail. Either single-node or single-edge cluster
		return false;
	}

	const FVector InitialDir = PCGExMath::GetNormal(Cluster->Nodes[StartNodeIndex].Position, Guide, Guide + FVector::UpVector);
	const int32 NextToStartIndex = Cluster->FindClosestNeighborInDirection(StartNodeIndex, InitialDir, 2);

	if (StartNodeIndex == -1)
	{
		// Fail. Either single-node or single-edge cluster
		return false;
	}

	TArray<int32> Path;
	TSet<int32> Visited;

	int32 PreviousIndex = StartNodeIndex;
	int32 NextIndex = NextToStartIndex;

	Path.Add(StartNodeIndex);
	Path.Add(NextToStartIndex);
	Visited.Add(NextToStartIndex);

	auto Move = [&](const TSet<int32>& InVisited)
	{
		const FVector DirToPreviousIndex = Cluster->GetEdgeDirection(NextIndex, PreviousIndex);
		PreviousIndex = NextIndex;
		NextIndex = Cluster->FindClosestNeighborLeft(NextIndex, DirToPreviousIndex, InVisited, 2);
	};
	
	const TSet<int32> TempSet = {PreviousIndex, NextIndex};
	Move(TempSet);

	while (NextIndex != -1)
	{
		if (NextIndex == StartNodeIndex) { break; } // Contour closed gracefully

		Path.Add(NextIndex);
		Visited.Add(NextIndex);

		//TODO: Skip neighbors that lead to dead ends
		
		if (Cluster->Nodes[NextIndex].AdjacentNodes.Contains(StartNodeIndex)) { break; } // End is in the immediate vicinity

		Move(Visited);
	}

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
	const TArray<FPCGPoint>& OriginPoints = PointIO->GetIn()->GetPoints();
	MutablePoints.SetNum(Path.Num());
	for (int i = 0; i < Path.Num(); i++) { MutablePoints[i] = OriginPoints[Cluster->Nodes[Path[i]].PointIndex]; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

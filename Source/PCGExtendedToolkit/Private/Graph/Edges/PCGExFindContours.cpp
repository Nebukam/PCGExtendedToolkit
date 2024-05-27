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

	PCGEX_DELETE_TARRAY(SeedTagGetters)
	PCGEX_DELETE_TARRAY(SeedForwardHandlers)

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

		if (Settings->bUseSeedAttributeToTagPath)
		{
			PCGEx::FLocalToStringGetter* NewTagGetter = new PCGEx::FLocalToStringGetter();
			NewTagGetter->Capture(Settings->SeedTagAttribute);
			NewTagGetter->SoftGrab(*SeedIO);
			Context->SeedTagGetters.Add(NewTagGetter);
		}

		PCGExDataBlending::FDataForwardHandler* NewForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, SeedIO);
		Context->SeedForwardHandlers.Add(NewForwardHandler);
	}

	Context->Paths = new PCGExData::FPointIOCollection();
	Context->Paths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	PCGEX_FWD(ProjectionSettings)

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

		Context->SetState(PCGExCluster::State_ProjectingCluster);
	}

	if (Context->IsState(PCGExCluster::State_ProjectingCluster))
	{
		if (!Context->ProjectCluster()) { return false; }

		for (int i = 0; i < Context->Seeds->Pairs.Num(); i++)
		{
			Context->GetAsyncManager()->Start<FPCGExFindContourTask>(i, &Context->Paths->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput), Context->CurrentCluster);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
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
	const int32 StartNodeIndex = Cluster->FindClosestNode(Guide, Settings->SeedPicking.PickingMethod, 2);

	if (StartNodeIndex == -1)
	{
		// Fail. Either single-node or single-edge cluster
		return false;
	}

	const FVector SeedPosition = Cluster->Nodes[StartNodeIndex].Position;
	if (!Settings->SeedPicking.WithinDistance(SeedPosition, Guide))
	{
		// Fail. Not within radius.
		return false;
	}

	const FVector InitialDir = PCGExMath::GetNormal(SeedPosition, Guide, Guide + FVector::UpVector);
	const int32 NextToStartIndex = Cluster->FindClosestNeighborInDirection(StartNodeIndex, InitialDir, 2);

	if (NextToStartIndex == -1)
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


	TSet<int32> Exclusion = {PreviousIndex, NextIndex};
	PreviousIndex = NextToStartIndex;
	NextIndex = Context->ClusterProjection->FindNextAdjacentNode(Settings->OrientationMode, NextToStartIndex, StartNodeIndex, Exclusion, 2);

	while (NextIndex != -1)
	{
		if (NextIndex == StartNodeIndex) { break; } // Contour closed gracefully

		const PCGExCluster::FNode& CurrentNode = Cluster->Nodes[NextIndex];

		Path.Add(NextIndex);

		if (CurrentNode.AdjacentNodes.Contains(StartNodeIndex)) { break; } // End is in the immediate vicinity

		Exclusion.Empty();
		if (CurrentNode.AdjacentNodes.Num() > 1) { Exclusion.Add(PreviousIndex); }

		const int32 FromIndex = PreviousIndex;
		PreviousIndex = NextIndex;
		NextIndex = Context->ClusterProjection->FindNextAdjacentNode(Settings->OrientationMode, NextIndex, FromIndex, Exclusion, 1);
	}

	PCGExGraph::CleanupVtxData(PointIO);

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
	const TArray<FPCGPoint>& OriginPoints = PointIO->GetIn()->GetPoints();
	MutablePoints.SetNumUninitialized(Path.Num());
	for (int i = 0; i < Path.Num(); i++) { MutablePoints[i] = OriginPoints[Cluster->Nodes[Path[i]].PointIndex]; }

	if (Settings->bUseSeedAttributeToTagPath)
	{
		PCGEx::FLocalToStringGetter* TagGetter = Context->SeedTagGetters[TaskIndex];
		if (TagGetter->bEnabled)
		{
			for (int i = 0; i < Guides.Num(); i++) { PointIO->Tags->RawTags.Add(TagGetter->SoftGet(Context->Seeds->Pairs[TaskIndex]->GetInPoint(i), TEXT(""))); }
		}

		Context->SeedForwardHandlers[TaskIndex]->Forward(0, PointIO);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

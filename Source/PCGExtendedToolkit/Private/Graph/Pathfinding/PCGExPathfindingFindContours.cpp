// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindContours.h"

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
	PCGEX_PIN_POINTS(PCGExPathfinding::SourceSeedsLabel, "Seeds associated with the main input points", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindContoursSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Contours", Required, {})
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

	ProjectedSeeds.Empty();
}


bool FPCGExFindContoursElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	Context->Seeds = Context->TryGetSingleInput(PCGExPathfinding::SourceSeedsLabel, true);
	if (!Context->Seeds) { return false; }

	if (Settings->bUseSeedAttributeToTagPath)
	{
		PCGEx::FLocalToStringGetter* NewTagGetter = new PCGEx::FLocalToStringGetter();
		NewTagGetter->Capture(Settings->SeedTagAttribute);
		NewTagGetter->SoftGrab(*Context->Seeds);
		Context->SeedTagGetters.Add(NewTagGetter);
	}

	PCGExDataBlending::FDataForwardHandler* NewForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, Context->Seeds);
	Context->SeedForwardHandlers.Add(NewForwardHandler);


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
		Context->SetState(PCGExMT::State_ProcessingTargets);
	}

	if (Context->IsState(PCGExMT::State_ProcessingTargets))
	{
		const TArray<FPCGPoint>& Seeds = Context->Seeds->GetIn()->GetPoints();

		auto Initialize = [&]()
		{
			Context->ProjectedSeeds.SetNumUninitialized(Seeds.Num());
		};

		auto ProjectSeed = [&](const int32 Index)
		{
			Context->ProjectedSeeds[Index] = Settings->ProjectionSettings.Project(Seeds[Index].Transform.GetLocation());
		};

		if (!Context->Process(Initialize, ProjectSeed, Seeds.Num())) { return false; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExFindContours::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExFindContours::FProcessor>* NewBatch) { return; },
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->Paths->OutputTo(Context);
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}


namespace PCGExFindContours
{
	bool FPCGExFindContourTask::ExecuteTask()
	{
		FPCGExFindContoursContext* Context = static_cast<FPCGExFindContoursContext*>(Manager->Context);
		PCGEX_SETTINGS(FindContours)

		const FVector Guide = Context->ProjectedSeeds[TaskIndex];
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

			if (CurrentNode.IsAdjacentTo(StartNodeIndex)) { break; } // End is in the immediate vicinity

			Exclusion.Empty();
			if (CurrentNode.Adjacency.Num() > 1) { Exclusion.Add(PreviousIndex); }

			const int32 FromIndex = PreviousIndex;
			PreviousIndex = NextIndex;
			NextIndex = Context->ClusterProjection->FindNextAdjacentNode(Settings->OrientationMode, NextIndex, FromIndex, Exclusion, 1);
		}

		PCGExGraph::CleanupClusterTags(PointIO, true);
		PCGExGraph::CleanupVtxData(PointIO);

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		const TArray<FPCGPoint>& OriginPoints = PointIO->GetIn()->GetPoints();
		MutablePoints.SetNumUninitialized(Path.Num());
		for (int i = 0; i < Path.Num(); i++) { MutablePoints[i] = OriginPoints[Cluster->Nodes[Path[i]].PointIndex]; }

		if (Settings->bUseSeedAttributeToTagPath)
		{
			PCGEx::FLocalToStringGetter* TagGetter = Context->SeedTagGetters[TaskIndex];
			if (TagGetter->bEnabled) { PointIO->Tags->RawTags.Add(TagGetter->SoftGet(Context->Seeds->GetInPoint(TaskIndex), FName(NAME_None).ToString())); }

			Context->SeedForwardHandlers[TaskIndex]->Forward(0, PointIO);
		}

		return true;
	}

	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessor(InVtx, InEdges)
	{
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(ClusterProjection)
		ProjectionSettings.Cleanup();
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		ProjectionSettings.Init(VtxIO);
		ClusterProjection = new PCGExCluster::FClusterProjection(Cluster, &ProjectionSettings);

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		

		for (int i = 0; i < TypedContext->ProjectedSeeds.Num(); i++)
		{
			AsyncManagerPtr->Start<FPCGExFindContourTask>(i, &TypedContext->Paths->Emplace_GetRef(*VtxIO, PCGExData::EInit::NewOutput), Cluster);
		}
	}

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node)
	{
		ClusterProjection->Nodes[Node.NodeIndex].Project(Cluster, &ProjectionSettings);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

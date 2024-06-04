// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingGrowPaths.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"
#include "Algo/Reverse.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingGrowPathsElement"
#define PCGEX_NAMESPACE PathfindingGrowPaths

#define PCGEX_GROWTH_GRAB_SINGLE_FIELD(_NAME, _SOURCE)\
Context->_NAME##Getter = new PCGEx::FLocalSingleFieldGetter();\
Context->_NAME##Getter->Capture(Settings->_NAME##Attribute);\
if (!Context->_NAME##Getter->Grab(_SOURCE)){\
	PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified " #_NAME " attribute."));\
	return false;}

#define PCGEX_GROWTH_GRAB_VECTOR_FIELD(_NAME, _SOURCE)\
Context->_NAME##Getter = new PCGEx::FLocalVectorGetter();\
Context->_NAME##Getter->Capture(Settings->_NAME##Attribute);\
if (!Context->_NAME##Getter->Grab(_SOURCE)){\
	PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified " #_NAME " attribute."));\
	return false;}

#if WITH_EDITOR
void UPCGExPathfindingGrowPathsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

int32 PCGExGrow::FGrowth::FindNextGrowthNodeIndex()
{
	if (Iteration + 1 > SoftMaxIterations)
	{
		NextGrowthIndex = -1;
		return NextGrowthIndex;
	}

	const PCGExCluster::FNode& CurrentNode = Context->CurrentCluster->Nodes[LastGrowthIndex];

	double BestScore = TNumericLimits<double>::Max();
	NextGrowthIndex = -1;

	for (const int32& AdjacentNodeIndex : CurrentNode.AdjacentNodes)
	{
		const PCGExCluster::FNode& OtherNode = Context->CurrentCluster->Nodes[AdjacentNodeIndex];
		const int32 EdgeIndex = CurrentNode.GetEdgeIndex(OtherNode.NodeIndex);

		if (Settings->bUseNoGrowth)
		{
			bool bNoGrowth = Context->NoGrowthGetter->SafeGet(OtherNode.PointIndex, Settings->bInvertNoGrowth);
			if (Settings->bInvertNoGrowth) { bNoGrowth = !bNoGrowth; }

			if (bNoGrowth) { continue; }
		}

		if (Path.Contains(AdjacentNodeIndex)) { continue; }

		/*
		// TODO : Implement
		if (Settings->VisitedStopThreshold > 0 && Context->GlobalExtraWeights &&
			Context->GlobalExtraWeights->GetExtraWeight(AdjacentNodeIndex, EdgeIndex) > Settings->VisitedStopThreshold)
		{
			continue;
		}
		*/

		if (const double Score = GetGrowthScore(
				CurrentNode, OtherNode, Context->CurrentCluster->Edges[EdgeIndex]);
			Score < BestScore)
		{
			BestScore = Score;
			NextGrowthIndex = OtherNode.NodeIndex;
		}
	}


	return NextGrowthIndex;
}

bool PCGExGrow::FGrowth::Grow()
{
	if (NextGrowthIndex <= -1 || Path.Contains(NextGrowthIndex)) { return false; }

	const PCGExCluster::FNode& CurrentNode = Context->CurrentCluster->Nodes[LastGrowthIndex];
	const PCGExCluster::FNode& NextNode = Context->CurrentCluster->Nodes[NextGrowthIndex];

	Metrics.Add(NextNode.Position);
	if (MaxDistance > 0 && Metrics.Length > MaxDistance) { return false; }

	Context->HeuristicsHandler->FeedbackScore(NextNode, Context->CurrentCluster->Edges[CurrentNode.GetEdgeIndex(NextNode.NodeIndex)]);

	Iteration++;
	Path.Add(NextGrowthIndex);
	LastGrowthIndex = NextGrowthIndex;

	if (Settings->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
	{
		if (Settings->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
		{
			SoftMaxIterations = Context->NumIterationsGetter->Values[NextNode.PointIndex];
		}
		else if (Settings->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
		{
			SoftMaxIterations += Context->NumIterationsGetter->Values[NextNode.PointIndex];
		}
	}

	if (Settings->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
	{
		if (Settings->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
		{
			GrowthDirection = Context->GrowthDirectionGetter->Values[NextNode.PointIndex];
		}
		else if (Settings->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
		{
			GrowthDirection = (GrowthDirection + Context->GrowthDirectionGetter->Values[NextNode.PointIndex]).GetSafeNormal();
		}
	}

	GoalNode->Position = NextNode.Position + GrowthDirection * 10000;

	if (Settings->bUseGrowthStop)
	{
		bool bStopGrowth = Context->GrowthStopGetter->SafeGet(NextNode.PointIndex, Settings->bInvertGrowthStop);
		if (Settings->bInvertGrowthStop) { bStopGrowth = !bStopGrowth; }
		if (bStopGrowth) { SoftMaxIterations = -1; }
	}

	return true;
}

void PCGExGrow::FGrowth::Write()
{
	const PCGExData::FPointIO& VtxPoints = *Context->CurrentIO;
	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(VtxPoints.GetIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num());

	for (const int32 VtxIndex : Path)
	{
		MutablePoints.Add(InPoints[Context->CurrentCluster->Nodes[VtxIndex].PointIndex]);
	}

	PathPoints.Tags->Append(VtxPoints.Tags);

	if (Settings->bUseSeedAttributeToTagPath)
	{
		PathPoints.Tags->RawTags.Add(Context->TagValueGetter->SoftGet(Context->SeedsPoints->GetInPoint(SeedPointIndex), TEXT("")));
	}

	Context->SeedForwardHandler->Forward(SeedPointIndex, &PathPoints);
}

void PCGExGrow::FGrowth::Init()
{
	SeedNode = &Context->CurrentCluster->Nodes[LastGrowthIndex];
	GoalNode = new PCGExCluster::FNode();
	GoalNode->Position = SeedNode->Position + GrowthDirection * 100;
	Metrics.Reset(SeedNode->Position);
}

double PCGExGrow::FGrowth::GetGrowthScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge) const
{
	return Context->HeuristicsHandler->GetEdgeScore(From, To, Edge, *SeedNode, *GoalNode);
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExPathfinding::SourceSeedsLabel, "Seed points to start growth from.", Required, {})
	PCGEX_PIN_PARAMS(PCGExPathfinding::SourceHeuristicsLabel, "Heuristics.", Normal, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathfindingGrowPaths)

void UPCGExPathfindingGrowPathsSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(SearchAlgorithm, UPCGExSearchAStar)
	PCGEX_OPERATION_DEFAULT(Heuristics, UPCGExHeuristicDistance)
}

FPCGExPathfindingGrowPathsContext::~FPCGExPathfindingGrowPathsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(HeuristicsHandler)

	PCGEX_DELETE_TARRAY(Growths)

	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE(NumBranchesGetter)
	PCGEX_DELETE(NumIterationsGetter)
	PCGEX_DELETE(GrowthDirectionGetter)
	PCGEX_DELETE(GrowthMaxDistanceGetter)
	PCGEX_DELETE(TagValueGetter)

	PCGEX_DELETE(GrowthStopGetter)
	PCGEX_DELETE(NoGrowthGetter)
}


bool FPCGExPathfindingGrowPathsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

	if (TArray<FPCGTaggedData> Seeds = InContext->InputData.GetInputsByPin(PCGExPathfinding::SourceSeedsLabel);
		Seeds.Num() > 0)
	{
		const FPCGTaggedData& SeedsSource = Seeds[0];
		Context->SeedsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, SeedsSource);
	}

	if (!Context->SeedsPoints || Context->SeedsPoints->GetNum() == 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Seed Points."));
		return false;
	}

	Context->HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(Context);

	Context->OutputPaths = new PCGExData::FPointIOCollection();

	if (Settings->NumIterations == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB_SINGLE_FIELD(NumIterations, *Context->SeedsPoints)
	}

	if (Settings->SeedNumBranches == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB_SINGLE_FIELD(NumBranches, *Context->SeedsPoints)
	}

	if (Settings->GrowthDirection == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB_VECTOR_FIELD(GrowthDirection, *Context->SeedsPoints)
	}

	if (Settings->GrowthMaxDistance == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB_SINGLE_FIELD(GrowthMaxDistance, *Context->SeedsPoints)
	}

	Context->GrowthStopGetter = new PCGEx::FLocalBoolGetter();
	Context->GrowthStopGetter->Capture(Settings->GrowthStopAttribute);

	Context->NoGrowthGetter = new PCGEx::FLocalBoolGetter();
	Context->NoGrowthGetter->Capture(Settings->NoGrowthAttribute);

	if (Settings->bUseSeedAttributeToTagPath)
	{
		Context->TagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->TagValueGetter->Capture(Settings->SeedTagAttribute);
		if (!Context->TagValueGetter->SoftGrab(*Context->SeedsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on seed points."));
			return false;
		}
	}

	Context->SeedForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, Context->SeedsPoints);

	return true;
}

bool FPCGExPathfindingGrowPathsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingGrowPathsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

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
				return false;
			}

			if (Settings->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
			{
				PCGEX_DELETE(Context->NumIterationsGetter)
				PCGEX_GROWTH_GRAB_SINGLE_FIELD(NumIterations, *Context->CurrentIO)
			}

			if (Settings->SeedNumBranches == EPCGExGrowthValueSource::VtxAttribute)
			{
				PCGEX_DELETE(Context->NumBranchesGetter)
				PCGEX_GROWTH_GRAB_SINGLE_FIELD(NumBranches, *Context->CurrentIO)
			}

			if (Settings->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
			{
				PCGEX_DELETE(Context->GrowthDirectionGetter)
				PCGEX_GROWTH_GRAB_VECTOR_FIELD(GrowthDirection, *Context->CurrentIO)
			}

			if (Settings->GrowthMaxDistance == EPCGExGrowthValueSource::VtxAttribute)
			{
				PCGEX_DELETE(Context->GrowthMaxDistanceGetter)
				PCGEX_GROWTH_GRAB_SINGLE_FIELD(GrowthMaxDistance, *Context->CurrentIO)
			}

			Context->GrowthStopGetter->Grab(*Context->CurrentIO);
			Context->NoGrowthGetter->Grab(*Context->CurrentIO);

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE_TARRAY(Context->Growths)
		Context->QueuedGrowths.Empty();

		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		if (Settings->bUseOctreeSearch) { Context->CurrentCluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }

		Context->HeuristicsHandler->PrepareForCluster(Context->GetAsyncManager(), Context->CurrentCluster);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		Context->HeuristicsHandler->CompleteClusterPreparation();

		// Find all growth points
		const int32 SeedCount = Context->SeedsPoints->GetNum();
		for (int i = 0; i < SeedCount; i++)
		{
			const FVector SeedPosition = Context->SeedsPoints->GetInPoint(i).Transform.GetLocation();
			const int32 NodeIndex = Context->CurrentCluster->FindClosestNode(SeedPosition, Settings->SeedPicking.PickingMethod, 1);

			if (NodeIndex == -1) { continue; }

			const PCGExCluster::FNode& Node = Context->CurrentCluster->Nodes[NodeIndex];
			if (!Settings->SeedPicking.WithinDistance(Node.Position, SeedPosition) ||
				Node.AdjacentNodes.IsEmpty()) { continue; }

			double NumIterations = 0;
			double GrowthNumBranches = 0;
			FVector GrowthDirection = FVector::UpVector;
			double GrowthMaxDistance = 0;

			switch (Settings->SeedNumBranches)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				GrowthNumBranches = Settings->NumBranchesConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				GrowthNumBranches = Context->NumBranchesGetter->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthNumBranches = Context->NumBranchesGetter->Values[Node.PointIndex];
				break;
			}

			switch (Settings->NumIterations)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				NumIterations = Settings->NumIterationsConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				NumIterations = Context->NumIterationsGetter->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				NumIterations = Context->NumIterationsGetter->Values[Node.PointIndex];
				break;
			}

			switch (Settings->GrowthMaxDistance)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				GrowthMaxDistance = Settings->GrowthMaxDistanceConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				GrowthMaxDistance = Context->GrowthMaxDistanceGetter->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthMaxDistance = Context->GrowthMaxDistanceGetter->Values[Node.PointIndex];
				break;
			}

			switch (Settings->GrowthDirection)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				GrowthDirection = Settings->GrowthDirectionConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				GrowthDirection = Context->GrowthDirectionGetter->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthDirection = Context->GrowthDirectionGetter->Values[Node.PointIndex];
				break;
			}

			if (GrowthNumBranches <= 0 || NumIterations <= 0) { continue; }

			if (Settings->SeedNumBranchesMean == EPCGExMeanMeasure::Relative)
			{
				GrowthNumBranches = FMath::Max(1, static_cast<double>(Node.AdjacentNodes.Num()) * GrowthNumBranches);
			}

			for (int j = 0; j < GrowthNumBranches; j++)
			{
				PCGExGrow::FGrowth* NewGrowth = new PCGExGrow::FGrowth(Context, Settings, NumIterations, Node.NodeIndex, GrowthDirection);
				NewGrowth->MaxDistance = GrowthMaxDistance;
				NewGrowth->SeedPointIndex = i;

				if (!(NewGrowth->FindNextGrowthNodeIndex() != -1 && NewGrowth->Grow()))
				{
					PCGEX_DELETE(NewGrowth)
					continue;
				}

				Context->Growths.Add(NewGrowth);
				Context->QueuedGrowths.Add(NewGrowth);
			}
		}

		Context->SetAsyncState(PCGExPathfinding::State_Pathfinding);
	}

	if (Context->IsState(PCGExPathfinding::State_Pathfinding))
	{
		if (Settings->GrowthMode == EPCGExGrowthIterationMode::Parallel)
		{
			for (PCGExGrow::FGrowth* Growth : Context->QueuedGrowths)
			{
				while (Growth->FindNextGrowthNodeIndex() != -1 && Growth->Grow())
				{
				}
			}

			Context->QueuedGrowths.Empty();
		}
		else
		{
			while (!Context->QueuedGrowths.IsEmpty())
			{
				for (int i = 0; i < Context->QueuedGrowths.Num(); i++)
				{
					PCGExGrow::FGrowth* Growth = Context->QueuedGrowths[i];

					Growth->FindNextGrowthNodeIndex();

					if (!Growth->Grow())
					{
						Context->QueuedGrowths.RemoveAt(i);
						i--;
					}
				}
			}
		}

		Context->SetState(PCGExGraph::State_BuildingClusters);
	}

	if (Context->IsState(PCGExGraph::State_BuildingClusters))
	{
		for (PCGExGrow::FGrowth* Growth : Context->Growths) { Growth->Write(); }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}


#undef PCGEX_GROWTH_GRAB_SINGLE_FIELD
#undef PCGEX_GROWTH_GRAB_VECTOR_FIELD

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

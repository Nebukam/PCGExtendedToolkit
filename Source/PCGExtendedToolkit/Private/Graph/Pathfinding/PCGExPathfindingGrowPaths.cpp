// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\..\Public\Graph\Pathfinding\PCGExPathfindingGrowPaths.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

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
	if (Heuristics) { Heuristics->UpdateUserFacingInfos(); }
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

int32 PCGExGrow::FGrowth::FindNextGrowthNodeIndex()
{
	if (Iteration + 1 > MaxIterations)
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

		if (Path.Contains(AdjacentNodeIndex)) { continue; }

		if (Settings->VisitedStopThreshold > 0 && Context->GlobalExtraWeights &&
			Context->GlobalExtraWeights->GetExtraWeight(AdjacentNodeIndex, EdgeIndex) > Settings->VisitedStopThreshold)
		{
			continue;
		}

		if (const double Score = Context->GetGrowthScore(
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

	if (Context->GlobalExtraWeights)
	{
		Context->GlobalExtraWeights->AddPointWeight(NextGrowthIndex, Context->Heuristics->ReferenceWeight * Settings->VisitedPointsWeightFactor);
		Context->GlobalExtraWeights->AddEdgeWeight(CurrentNode.GetEdgeIndex(NextNode.NodeIndex), Context->Heuristics->ReferenceWeight * Settings->VisitedEdgesWeightFactor);
	}

	Iteration++;
	Path.Add(NextGrowthIndex);
	LastGrowthIndex = NextGrowthIndex;
	return true;
}

void PCGExGrow::FGrowth::Write()
{
	PCGExData::FPointIO& VtxPoints = *Context->CurrentIO;
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
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = FTEXT("Seed points to start growth from.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif

	return PinProperties;
}

double FPCGExPathfindingGrowPathsContext::GetGrowthScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge) const
{
	return Heuristics->GetEdgeScore(From, To, Edge, From, To) +
		HeuristicsModifiers->GetScore(To.PointIndex, Edge.PointIndex) +
		(GlobalExtraWeights ? GlobalExtraWeights->GetExtraWeight(From.NodeIndex, Edge.EdgeIndex) : 0);
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

	if (HeuristicsModifiers) { HeuristicsModifiers->Cleanup(); }

	PCGEX_DELETE_TARRAY(Growths)

	PCGEX_DELETE(GlobalExtraWeights)
	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE(NumBranchesGetter)
	PCGEX_DELETE(NumIterationsGetter)
	PCGEX_DELETE(GrowthDirectionGetter)
	PCGEX_DELETE(GrowthMaxDistanceGetter)
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

	PCGEX_OPERATION_BIND(Heuristics, UPCGExHeuristicDistance)

	PCGEX_FWD(bWeightUpVisited)
	PCGEX_FWD(VisitedPointsWeightFactor)
	PCGEX_FWD(VisitedEdgesWeightFactor)

	Context->OutputPaths = new PCGExData::FPointIOCollection();

	Context->HeuristicsModifiers = const_cast<FPCGExHeuristicModifiersSettings*>(&Settings->HeuristicsModifiers);
	Context->HeuristicsModifiers->LoadCurves();

	Context->Heuristics->ReferenceWeight = Context->HeuristicsModifiers->ReferenceWeight;


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

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GlobalExtraWeights);
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

		Context->GetAsyncManager()->Start<FPCGExCompileModifiersTask>(0, Context->CurrentIO, Context->CurrentEdges, Context->HeuristicsModifiers);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		Context->Heuristics->PrepareForData(Context->CurrentCluster);

		if (Settings->bWeightUpVisited)
		{
			Context->GlobalExtraWeights = new PCGExPathfinding::FExtraWeights(
				Context->CurrentCluster,
				Context->VisitedPointsWeightFactor,
				Context->VisitedEdgesWeightFactor);
		}

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
				GrowthNumBranches = Context->NumBranchesGetter->SafeGet(i, 0);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthNumBranches = Context->NumBranchesGetter->SafeGet(Node.PointIndex, 0);
				break;
			}

			switch (Settings->NumIterations)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				NumIterations = Settings->NumIterationsConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				NumIterations = Context->NumIterationsGetter->SafeGet(i, 0);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				NumIterations = Context->NumIterationsGetter->SafeGet(Node.PointIndex, 0);
				break;
			}

			switch (Settings->GrowthMaxDistance)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				GrowthMaxDistance = Settings->GrowthMaxDistanceConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				GrowthMaxDistance = Context->GrowthMaxDistanceGetter->SafeGet(i, 0);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthMaxDistance = Context->GrowthMaxDistanceGetter->SafeGet(Node.PointIndex, 0);
				break;
			}

			switch (Settings->GrowthDirection)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				GrowthDirection = Settings->GrowthDirectionConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				GrowthDirection = Context->GrowthDirectionGetter->SafeGet(i, FVector::UpVector);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				GrowthDirection = Context->GrowthDirectionGetter->SafeGet(Node.PointIndex, FVector::UpVector);
				break;
			}

			if (GrowthNumBranches <= 0 || NumIterations <= 0) { continue; }

			if (Settings->SeedNumBranchesMean == EPCGExMeanMeasure::Relative)
			{
				GrowthNumBranches = FMath::Max(1, static_cast<double>(Node.AdjacentNodes.Num()) * GrowthNumBranches);
			}

			for (int j = 0; j < GrowthNumBranches; j++)
			{
				PCGExGrow::FGrowth* NewGrowth = new PCGExGrow::FGrowth(Context, Settings, NumIterations, Node.NodeIndex);

				NewGrowth->MaxDistance = GrowthMaxDistance;
				NewGrowth->GrowthDirection = GrowthDirection;
				NewGrowth->Metrics.Reset(Node.Position);

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
	}

	return Context->IsDone();
}

/*
bool FPCGExPlotClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingGrowPathsContext* Context = Manager->GetContext<FPCGExPathfindingGrowPathsContext>();

	const PCGExCluster::FCluster* Cluster = Context->CurrentCluster;
	TArray<int32> Path;

	PCGExPathfinding::FExtraWeights* ExtraWeights = GlobalExtraWeights;
	if (Context->bWeightUpVisited && !ExtraWeights)
	{
		ExtraWeights = new PCGExPathfinding::FExtraWeights(
			Cluster,
			Context->VisitedPointsWeightFactor,
			Context->VisitedEdgesWeightFactor);
	}

	const int32 NumPlots = PointIO->GetNum();

	for (int i = 1; i < NumPlots; i++)
	{
		FVector SeedPosition = PointIO->GetInPoint(i - 1).Transform.GetLocation();
		FVector GoalPosition = PointIO->GetInPoint(i).Transform.GetLocation();

		SeedPosition = GoalPosition;
	}

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	int32 LastIndex = -1;
	for (const int32 VtxIndex : Path)
	{
		if (VtxIndex < 0) // Plot point
		{
			MutablePoints.Add_GetRef(PointIO->GetInPoint((VtxIndex * -1) - 1)).MetadataEntry = PCGInvalidEntryKey;
			continue;
		}

		if (LastIndex == VtxIndex) { continue; } //Skip duplicates
		MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]);
		LastIndex = VtxIndex;
	}

	if (ExtraWeights != GlobalExtraWeights) { PCGEX_DELETE(ExtraWeights) }

	PathPoints.Tags->Append(PointIO->Tags);

	return true;
}
*/

#undef PCGEX_GROWTH_GRAB_SINGLE_FIELD
#undef PCGEX_GROWTH_GRAB_VECTOR_FIELD

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

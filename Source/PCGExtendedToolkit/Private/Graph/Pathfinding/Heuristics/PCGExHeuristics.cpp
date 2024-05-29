// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#include "Graph/Pathfinding/Heuristics/PCGExCreateHeuristicsModifier.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

PCGExHeuristics::THeuristicsHandler::THeuristicsHandler(FPCGExPointsProcessorContext* InContext)
{
	const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(PCGExPathfinding::InputHeuristicsLabel);

	TotalWeight = 0;
	HeuristicsModifiers = new FPCGExHeuristicModifiersSettings();

	TArray<FPCGExHeuristicModifierDescriptor> ModifierDescriptors;


	for (const FPCGTaggedData& InputState : Inputs)
	{
		if (const UPCGHeuristicsFactoryBase* OperationFactory = Cast<UPCGHeuristicsFactoryBase>(InputState.Data))
		{
			UPCGExHeuristicOperation* Operation = OperationFactory->CreateOperation();
			Operations.Add(Operation);
			InContext->RegisterOperation<UPCGExHeuristicOperation>(Operation);
			HeuristicsWeight += OperationFactory->WeightFactor;

			if (UPCGExHeuristicFeedback* Feedback = Cast<UPCGExHeuristicFeedback>(Operation))
			{
				Feedbacks.Add(Feedback);
				if (Feedback->bGlobalFeedback) { bHasGlobalFeedback = true; }
			}
		}
		else if (UPCGHeuristicsModiferFactory* ModifierFactory = Cast<UPCGHeuristicsModiferFactory>(InputState.Data))
		{
			HeuristicsModifiers->Modifiers.Add(FPCGExHeuristicModifier(ModifierFactory->Descriptor));
			PCGEX_LOAD_SOFTOBJECT(UCurveFloat, ModifierFactory->Descriptor.ScoreCurve, ModifierFactory->Descriptor.ScoreCurveObj, PCGEx::WeightDistributionLinear);
		}
	}
}

PCGExHeuristics::THeuristicsHandler::THeuristicsHandler(UPCGExHeuristicOperation* InSingleOperation)
{
	Operations.Add(InSingleOperation);
	HeuristicsWeight = 1;
}

PCGExHeuristics::THeuristicsHandler::~THeuristicsHandler()
{
	PCGEX_DELETE(HeuristicsModifiers)

	for (UPCGExHeuristicOperation* Operation : Operations)
	{
		Operation->Cleanup();
		Operation->ConditionalBeginDestroy();
	}

	Operations.Empty();
}

void PCGExHeuristics::THeuristicsHandler::PrepareForData(FPCGExAsyncManager* AsyncManager, PCGExCluster::FCluster* InCluster)
{
	CurrentCluster = InCluster;
	for (UPCGExHeuristicOperation* Operation : Operations) { Operation->PrepareForData(InCluster); }

	//HeuristicsModifiers->PrepareForData(*InCluster->PointsIO, *InCluster->EdgesIO); 
	AsyncManager->Start<PCGExHeuristicsTasks::FCompileModifiers>(0, InCluster->PointsIO, InCluster->EdgesIO, HeuristicsModifiers);
}

double PCGExHeuristics::THeuristicsHandler::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	double GScore = 0;
	for (const UPCGExHeuristicOperation* Op : Operations) { GScore += Op->GetGlobalScore(From, Seed, Goal); }
	return GScore / HeuristicsWeight;
}

double PCGExHeuristics::THeuristicsHandler::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	double EScore = 0;
	for (const UPCGExHeuristicOperation* Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal); }
	EScore += HeuristicsModifiers->GetScore(To.PointIndex, Edge.EdgeIndex);
	return EScore / TotalWeight;
}

void PCGExHeuristics::THeuristicsHandler::FeedbackPointScore(const PCGExCluster::FNode& Node)
{
	for (UPCGExHeuristicFeedback* Op : Feedbacks) { Op->FeedbackPointScore(Node); }
}

void PCGExHeuristics::THeuristicsHandler::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
{
	for (UPCGExHeuristicFeedback* Op : Feedbacks) { Op->FeedbackScore(Node, Edge); }
}

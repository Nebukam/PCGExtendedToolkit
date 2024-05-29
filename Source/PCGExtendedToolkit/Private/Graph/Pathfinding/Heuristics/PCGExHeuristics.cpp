// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#include "Graph/Pathfinding/Heuristics/PCGExCreateHeuristicsModifier.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

namespace PCGExHeuristics
{
	PCGExHeuristics::THeuristicsHandler::THeuristicsHandler(FPCGExPointsProcessorContext* InContext)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(PCGExPathfinding::InputHeuristicsLabel);

		TotalWeight = 0;
		HeuristicsModifiers = new FPCGExHeuristicModifiersSettings();

		TArray<FPCGExHeuristicModifierDescriptor> ModifierDescriptors;


		for (const FPCGTaggedData& InputState : Inputs)
		{
			if (UPCGHeuristicsFactoryBase* OperationFactory = Cast<UPCGHeuristicsFactoryBase>(InputState.Data))
			{
				UPCGExHeuristicOperation* Operation = nullptr;

				if (UPCGHeuristicsFactoryFeedback* FeedbackFactory = Cast<UPCGHeuristicsFactoryFeedback>(Operation))
				{
					if (FeedbackFactory->IsGlobal())
					{
						HasGlobalFeedback = true;
						Operation = OperationFactory->CreateOperation();
						UPCGExHeuristicFeedback* Feedback = Cast<UPCGExHeuristicFeedback>(Operation);
						Feedbacks.Add(Feedback);
					}
					else
					{
						bHasLocalFeedback = true;
						LocalFeedbackFactories.Add(OperationFactory);
						continue;
					}
				}
				else
				{
					Operation = OperationFactory->CreateOperation();
				}

				Operations.Add(Operation);
				InContext->RegisterOperation<UPCGExHeuristicOperation>(Operation);
				HeuristicsWeight += OperationFactory->WeightFactor;
			}
			else if (UPCGHeuristicsModiferFactory* ModifierFactory = Cast<UPCGHeuristicsModiferFactory>(InputState.Data))
			{
				HeuristicsModifiers->Modifiers.Add(FPCGExHeuristicModifier(ModifierFactory->Descriptor));
				PCGEX_LOAD_SOFTOBJECT(UCurveFloat, ModifierFactory->Descriptor.ScoreCurve, ModifierFactory->Descriptor.ScoreCurveObj, PCGEx::WeightDistributionLinear);
			}
		}

		if (Operations.IsEmpty() && HeuristicsModifiers->Modifiers.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing valid heuristics. Will use Shortest Distance as default."));
			UPCGExHeuristicOperation* DefaultHeuristics = Cast<UPCGExHeuristicOperation>(NewObject<UPCGHeuristicsFactoryShortestDistance>());
			Operations.Add(DefaultHeuristics);
			HeuristicsWeight += 1;
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
			PCGEX_DELETE_UOBJECT(Operation)
		}

		Operations.Empty();
	}

	void PCGExHeuristics::THeuristicsHandler::PrepareForCluster(FPCGExAsyncManager* AsyncManager, PCGExCluster::FCluster* InCluster)
	{
		CurrentCluster = InCluster;
		for (UPCGExHeuristicOperation* Operation : Operations) { Operation->PrepareForData(InCluster); }

		//HeuristicsModifiers->PrepareForData(*InCluster->PointsIO, *InCluster->EdgesIO); 
		AsyncManager->Start<PCGExHeuristicsTasks::FCompileModifiers>(0, InCluster->PointsIO, InCluster->EdgesIO, HeuristicsModifiers);
	}

	void PCGExHeuristics::THeuristicsHandler::CompleteClusterPreparation()
	{
		// Recompute weights
		TotalWeight = HeuristicsWeight;
	}

	double PCGExHeuristics::THeuristicsHandler::GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const FLocalFeedbackHandler* LocalFeedback) const
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
		const PCGExCluster::FNode& Goal,
		const FLocalFeedbackHandler* LocalFeedback) const
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

	PCGExHeuristics::FLocalFeedbackHandler* PCGExHeuristics::THeuristicsHandler::MakeLocalFeedbackHandler(PCGExCluster::FCluster* InCluster)
	{
		if (!LocalFeedbackFactories.IsEmpty()) { return nullptr; }
		PCGExHeuristics::FLocalFeedbackHandler* NewLocalFeedbackHandler = new FLocalFeedbackHandler();
		return new FLocalFeedbackHandler();
	}
}

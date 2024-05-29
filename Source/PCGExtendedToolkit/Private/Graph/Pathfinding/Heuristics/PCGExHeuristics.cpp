// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#include "Graph/Pathfinding/Heuristics/PCGExCreateHeuristicsModifier.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

namespace PCGExHeuristics
{
	double FLocalFeedbackHandler::GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
	{
		double GScore = 0;
		for (const UPCGExHeuristicFeedback* Feedback : Feedbacks) { GScore += Feedback->GetGlobalScore(From, Seed, Goal); }
		return GScore;
	}

	double FLocalFeedbackHandler::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
	{
		double EScore = 0;
		for (const UPCGExHeuristicFeedback* Feedback : Feedbacks) { EScore += Feedback->GetEdgeScore(From, To, Edge, Seed, Goal); }
		return EScore;
	}

	void FLocalFeedbackHandler::FeedbackPointScore(const PCGExCluster::FNode& Node)
	{
		for (UPCGExHeuristicFeedback* Feedback : Feedbacks) { Feedback->FeedbackPointScore(Node); }
	}

	void FLocalFeedbackHandler::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
	{
		for (UPCGExHeuristicFeedback* Feedback : Feedbacks) { Feedback->FeedbackScore(Node, Edge); }
	}

	PCGExHeuristics::THeuristicsHandler::THeuristicsHandler(FPCGExPointsProcessorContext* InContext, double InReferenceWeight):
		ReferenceWeight(InReferenceWeight)
	{
		TotalWeight = 0;
		HeuristicsModifiers = new FPCGExHeuristicModifiersSettings();
		HeuristicsModifiers->ReferenceWeight = ReferenceWeight;

		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(PCGExPathfinding::SourceHeuristicsLabel);
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
						Operation = OperationFactory->CreateOperation();
						UPCGExHeuristicFeedback* Feedback = Cast<UPCGExHeuristicFeedback>(Operation);
						Feedbacks.Add(Feedback);
					}
					else
					{
						LocalFeedbackFactories.Add(OperationFactory);
					}
				}
				else
				{
					Operation = OperationFactory->CreateOperation();
				}

				Operations.Add(Operation);
				Operation->ReferenceWeight = ReferenceWeight * OperationFactory->WeightFactor;
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
			UPCGExHeuristicDistance* DefaultHeuristics = NewObject<UPCGExHeuristicDistance>();
			DefaultHeuristics->ReferenceWeight = ReferenceWeight;
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
		Feedbacks.Empty();
	}

	bool PCGExHeuristics::THeuristicsHandler::PrepareForCluster(FPCGExAsyncManager* AsyncManager, PCGExCluster::FCluster* InCluster)
	{
		InCluster->ComputeEdgeLengths(true);
		
		CurrentCluster = InCluster;
		for (UPCGExHeuristicOperation* Operation : Operations)
		{
			Operation->PrepareForCluster(InCluster);
		}

		if (HeuristicsModifiers->Modifiers.IsEmpty())
		{
			HeuristicsModifiers->PrepareForData(*InCluster->PointsIO, *InCluster->EdgesIO);
			return false;
		}

		AsyncManager->Start<PCGExHeuristicsTasks::FCompileModifiers>(0, InCluster->PointsIO, InCluster->EdgesIO, HeuristicsModifiers);

		return true;
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
		if (LocalFeedback) { return (GScore + LocalFeedback->GetGlobalScore(From, Seed, Goal)) / (TotalWeight + LocalFeedback->TotalWeight); }
		return GScore / TotalWeight;
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
		if (LocalFeedback) { return (EScore + LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal)) / (TotalWeight + LocalFeedback->TotalWeight); }
		else { return EScore / TotalWeight; }
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

		for (const UPCGHeuristicsFactoryBase* Factory : LocalFeedbackFactories)
		{
			UPCGExHeuristicFeedback* Feedback = Cast<UPCGExHeuristicFeedback>(Factory->CreateOperation());
			Feedback->ReferenceWeight = ReferenceWeight * Factory->WeightFactor;
			
			NewLocalFeedbackHandler->Feedbacks.Add(Feedback);
			NewLocalFeedbackHandler->TotalWeight += Factory->WeightFactor;
			
			Feedback->PrepareForCluster(InCluster);
		}

		return NewLocalFeedbackHandler;
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

#define PCGEX_INIT_HEURISTIC_OPERATION(_OP, _FACTORY)\
_OP->PrimaryDataFacade = VtxDataFacade;\
_OP->SecondaryDataFacade = EdgeDataFacade;\
_OP->WeightFactor = _FACTORY->WeightFactor;\
_OP->ReferenceWeight = ReferenceWeight * _FACTORY->WeightFactor;

namespace PCGExHeuristics
{
	FHeuristicsHandler::FHeuristicsHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
		: ExecutionContext(InContext), VtxDataFacade(InVtxDataCache), EdgeDataFacade(InEdgeDataCache)
	{
		bIsValidHandler = BuildFrom(InContext, InFactories);
	}

	FHeuristicsHandler::~FHeuristicsHandler()
	{
		Operations.Empty();
		Feedbacks.Empty();
	}

	bool FHeuristicsHandler::BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
	{
		for (const UPCGExHeuristicsFactoryData* OperationFactory : InFactories)
		{
			TSharedPtr<FPCGExHeuristicOperation> Operation = nullptr;
			bool bIsFeedback = false;
			if (const UPCGExHeuristicsFactoryFeedback* FeedbackFactory = Cast<UPCGExHeuristicsFactoryFeedback>(OperationFactory))
			{
				bIsFeedback = true;

				if (!FeedbackFactory->IsGlobal())
				{
					LocalFeedbackFactories.Add(OperationFactory);
					continue;
				}
			}

			Operation = OperationFactory->CreateOperation(InContext);

			if (bIsFeedback) { Feedbacks.Add(StaticCastSharedPtr<FPCGExHeuristicFeedback>(Operation)); }
			Operations.Add(Operation);

			PCGEX_INIT_HEURISTIC_OPERATION(Operation, OperationFactory)

			Operation->BindContext(InContext);
		}

		if (Operations.IsEmpty())
		{
			if (!LocalFeedbackFactories.IsEmpty())
			{
				PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("Missing valid base heuristics : cannot work with feedback alone."))
			}
			else
			{
				PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("Missing valid base heuristics"))
			}

			return false;
		}

		return true;
	}

	void FHeuristicsHandler::PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
	{
		InCluster->ComputeEdgeLengths(true); // TODO : Make our own copy

		Cluster = InCluster;
		bUseDynamicWeight = false;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Operation : Operations)
		{
			Operation->PrepareForCluster(InCluster);
			if (Operation->bHasCustomLocalWeightMultiplier) { bUseDynamicWeight = true; }
		}
	}

	void FHeuristicsHandler::CompleteClusterPreparation()
	{
		TotalStaticWeight = 0;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { TotalStaticWeight += Op->WeightFactor; }
	}

	double FHeuristicsHandler::GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		double GScore = 0;
		double EWeight = TotalStaticWeight;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { GScore += Op->GetGlobalScore(From, Seed, Goal); }
		if (LocalFeedback)
		{
			GScore += LocalFeedback->GetGlobalScore(From, Seed, Goal);
			EWeight += LocalFeedback->TotalStaticWeight;
		}
		return GScore / EWeight;
	}

	double FHeuristicsHandler::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double EScore = 0;
		double EWeight = TotalStaticWeight;

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }

			if (LocalFeedback)
			{
				EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
				EWeight += LocalFeedback->TotalStaticWeight;
			}

			return EScore / EWeight;
		}

		EWeight = 0;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
			EWeight += (Op->WeightFactor * Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex));
		}

		if (LocalFeedback)
		{
			EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
			//EWeight += LocalFeedback->TotalStaticWeight;
		}

		return EScore / EWeight;
	}

	void FHeuristicsHandler::FeedbackPointScore(const PCGExCluster::FNode& Node)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Op : Feedbacks) { Op->FeedbackPointScore(Node); }
	}

	void FHeuristicsHandler::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Op : Feedbacks) { Op->FeedbackScore(Node, Edge); }
	}

	FVector FHeuristicsHandler::GetSeedUVW() const
	{
		FVector UVW = FVector::ZeroVector;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { UVW += Op->GetSeedUVW(); }
		return UVW;
	}

	FVector FHeuristicsHandler::GetGoalUVW() const
	{
		FVector UVW = FVector::ZeroVector;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { UVW += Op->GetGoalUVW(); }
		return UVW;
	}

	const PCGExCluster::FNode* FHeuristicsHandler::GetRoamingSeed()
	{
		if (RoamingSeedNode) { return RoamingSeedNode; }
		RoamingSeedNode = Cluster->GetRoamingNode(GetSeedUVW());
		return RoamingSeedNode;
	}

	const PCGExCluster::FNode* FHeuristicsHandler::GetRoamingGoal()
	{
		if (RoamingGoalNode) { return RoamingGoalNode; }
		RoamingGoalNode = Cluster->GetRoamingNode(GetGoalUVW());
		return RoamingGoalNode;
	}

	TSharedPtr<FLocalFeedbackHandler> FHeuristicsHandler::MakeLocalFeedbackHandler(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
	{
		if (LocalFeedbackFactories.IsEmpty()) { return nullptr; }

		PCGEX_MAKE_SHARED(NewLocalFeedbackHandler, FLocalFeedbackHandler, ExecutionContext)

		for (const UPCGExHeuristicsFactoryData* Factory : LocalFeedbackFactories)
		{
			TSharedPtr<FPCGExHeuristicFeedback> Feedback = StaticCastSharedPtr<FPCGExHeuristicFeedback>(Factory->CreateOperation(ExecutionContext));

			PCGEX_INIT_HEURISTIC_OPERATION(Feedback, Factory)

			NewLocalFeedbackHandler->Feedbacks.Add(Feedback);
			NewLocalFeedbackHandler->TotalStaticWeight += Factory->WeightFactor;

			Feedback->PrepareForCluster(InCluster);
		}

		return NewLocalFeedbackHandler;
	}
}

#undef PCGEX_INIT_HEURISTIC_OPERATION

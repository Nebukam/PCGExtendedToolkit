// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHeuristicsHandler.h"

#include "Clusters/PCGExCluster.h"
#include "Heuristics/PCGExHeuristicFeedback.h"
#include "Core/PCGExHeuristicOperation.h"

#define PCGEX_INIT_HEURISTIC_OPERATION(_OP, _FACTORY)\
_OP->PrimaryDataFacade = VtxDataFacade;\
_OP->SecondaryDataFacade = EdgeDataFacade;\
_OP->WeightFactor = _FACTORY->WeightFactor;\
_OP->ReferenceWeight = ReferenceWeight * _FACTORY->WeightFactor;

namespace PCGExHeuristics
{
	double FLocalFeedbackHandler::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const
	{
		double GScore = 0;
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Feedback : Feedbacks) { GScore += Feedback->GetGlobalScore(From, Seed, Goal); }
		return GScore;
	}

	double FLocalFeedbackHandler::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double EScore = 0;
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Feedback : Feedbacks) { EScore += Feedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }
		return EScore;
	}

	void FLocalFeedbackHandler::FeedbackPointScore(const PCGExClusters::FNode& Node)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Feedback : Feedbacks) { Feedback->FeedbackPointScore(Node); }
	}

	void FLocalFeedbackHandler::FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Feedback : Feedbacks) { Feedback->FeedbackScore(Node, Edge); }
	}

	FHandler::FHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
		: ExecutionContext(InContext), VtxDataFacade(InVtxDataCache), EdgeDataFacade(InEdgeDataCache)
	{
		bIsValidHandler = BuildFrom(InContext, InFactories);
	}

	FHandler::~FHandler()
	{
		Operations.Empty();
		Feedbacks.Empty();
	}

	bool FHandler::BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
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

	void FHandler::PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster)
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

	void FHandler::CompleteClusterPreparation()
	{
		TotalStaticWeight = 0;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { TotalStaticWeight += Op->WeightFactor; }
	}

	double FHandler::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
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

	double FHandler::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
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

	void FHandler::FeedbackPointScore(const PCGExClusters::FNode& Node)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Op : Feedbacks) { Op->FeedbackPointScore(Node); }
	}

	void FHandler::FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge)
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Op : Feedbacks) { Op->FeedbackScore(Node, Edge); }
	}

	FVector FHandler::GetSeedUVW() const
	{
		FVector UVW = FVector::ZeroVector;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { UVW += Op->GetSeedUVW(); }
		return UVW;
	}

	FVector FHandler::GetGoalUVW() const
	{
		FVector UVW = FVector::ZeroVector;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { UVW += Op->GetGoalUVW(); }
		return UVW;
	}

	const PCGExClusters::FNode* FHandler::GetRoamingSeed()
	{
		if (RoamingSeedNode) { return RoamingSeedNode; }
		RoamingSeedNode = Cluster->GetRoamingNode(GetSeedUVW());
		return RoamingSeedNode;
	}

	const PCGExClusters::FNode* FHandler::GetRoamingGoal()
	{
		if (RoamingGoalNode) { return RoamingGoalNode; }
		RoamingGoalNode = Cluster->GetRoamingNode(GetGoalUVW());
		return RoamingGoalNode;
	}

	TSharedPtr<FLocalFeedbackHandler> FHandler::MakeLocalFeedbackHandler(const TSharedPtr<const PCGExClusters::FCluster>& InCluster)
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

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"


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
		for (UPCGExHeuristicOperation* Op : Operations) { ExecutionContext->ManagedObjects->Destroy(Op); }

		Operations.Empty();
		Feedbacks.Empty();
	}

	bool FHeuristicsHandler::BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
	{
		for (const UPCGExHeuristicsFactoryData* OperationFactory : InFactories)
		{
			UPCGExHeuristicOperation* Operation = nullptr;
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

			if (bIsFeedback) { Feedbacks.Add(Cast<UPCGExHeuristicFeedback>(Operation)); }
			Operations.Add(Operation);

			PCGEX_INIT_HEURISTIC_OPERATION(Operation, OperationFactory)

			Operation->BindContext(InContext);
		}

		if (Operations.IsEmpty())
		{
			if (!LocalFeedbackFactories.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing valid base heuristics : cannot work with feedback alone."));
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing valid base heuristics"));
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
		for (UPCGExHeuristicOperation* Operation : Operations)
		{
			Operation->PrepareForCluster(InCluster);
			if (Operation->bHasCustomLocalWeightMultiplier) { bUseDynamicWeight = true; }
		}
	}

	void FHeuristicsHandler::CompleteClusterPreparation()
	{
		TotalStaticWeight = 0;
		for (const UPCGExHeuristicOperation* Op : Operations) { TotalStaticWeight += Op->WeightFactor; }
	}

	TSharedPtr<FLocalFeedbackHandler> FHeuristicsHandler::MakeLocalFeedbackHandler(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
	{
		if (LocalFeedbackFactories.IsEmpty()) { return nullptr; }

		PCGEX_MAKE_SHARED(NewLocalFeedbackHandler, FLocalFeedbackHandler, ExecutionContext)

		for (const UPCGExHeuristicsFactoryData* Factory : LocalFeedbackFactories)
		{
			UPCGExHeuristicFeedback* Feedback = Cast<UPCGExHeuristicFeedback>(Factory->CreateOperation(ExecutionContext));

			PCGEX_INIT_HEURISTIC_OPERATION(Feedback, Factory)

			NewLocalFeedbackHandler->Feedbacks.Add(Feedback);
			NewLocalFeedbackHandler->TotalStaticWeight += Factory->WeightFactor;

			Feedback->PrepareForCluster(InCluster);
		}

		return NewLocalFeedbackHandler;
	}
}

#undef PCGEX_INIT_HEURISTIC_OPERATION

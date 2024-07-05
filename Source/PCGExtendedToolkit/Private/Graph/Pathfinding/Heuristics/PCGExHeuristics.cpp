// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

namespace PCGExHeuristics
{
	THeuristicsHandler::THeuristicsHandler(FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade)
		: VtxDataFacade(InVtxDataFacade), EdgeDataFacade(InEdgeDataFacade)
	{
		TArray<UPCGExHeuristicsFactoryBase*> ContextFactories;
		PCGExFactories::GetInputFactories(
			InContext, PCGExGraph::SourceHeuristicsLabel, ContextFactories,
			{PCGExFactories::EType::Heuristics}, false);
		BuildFrom(InContext, ContextFactories);
	}

	THeuristicsHandler::THeuristicsHandler(FPCGContext* InContext, PCGExData::FFacade* InVtxDataCache, PCGExData::FFacade* InEdgeDataCache, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories)
		: VtxDataFacade(InVtxDataCache), EdgeDataFacade(InEdgeDataCache)
	{
		BuildFrom(InContext, InFactories);
	}

	THeuristicsHandler::~THeuristicsHandler()
	{
		for (UPCGExHeuristicOperation* Operation : Operations)
		{
			Operation->Cleanup();
			PCGEX_DELETE_UOBJECT(Operation)
		}

		Operations.Empty();
		Feedbacks.Empty();
	}

	void THeuristicsHandler::BuildFrom(FPCGContext* InContext, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories)
	{
		for (const UPCGExHeuristicsFactoryBase* OperationFactory : InFactories)
		{
			UPCGExHeuristicOperation* Operation = nullptr;

			if (const UPCGHeuristicsFactoryFeedback* FeedbackFactory = Cast<UPCGHeuristicsFactoryFeedback>(OperationFactory))
			{
				if (!FeedbackFactory->IsGlobal())
				{
					LocalFeedbackFactories.Add(OperationFactory);
					continue;
				}

				Operation = OperationFactory->CreateOperation();
				Feedbacks.Add(Cast<UPCGExHeuristicFeedback>(Operation));
			}
			else
			{
				Operation = OperationFactory->CreateOperation();
			}

			Operations.Add(Operation);
			Operation->PrimaryDataFacade = VtxDataFacade;
			Operation->SecondaryDataFacade = EdgeDataFacade;
			Operation->WeightFactor = OperationFactory->WeightFactor;
			Operation->ReferenceWeight = ReferenceWeight * Operation->WeightFactor;
			Operation->BindContext(InContext);
		}


		if (Operations.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing valid heuristics. Will use Shortest Distance as default. (Local feedback heuristics don't count)"));
			UPCGExHeuristicDistance* DefaultHeuristics = NewObject<UPCGExHeuristicDistance>();
			DefaultHeuristics->ReferenceWeight = ReferenceWeight;
			Operations.Add(DefaultHeuristics);
		}
	}

	void THeuristicsHandler::PrepareForCluster(PCGExCluster::FCluster* InCluster)
	{
		InCluster->ComputeEdgeLengths(true);

		CurrentCluster = InCluster;
		bUseDynamicWeight = false;
		for (UPCGExHeuristicOperation* Operation : Operations)
		{
			Operation->PrepareForCluster(InCluster);
			if (Operation->bHasCustomLocalWeightMultiplier) { bUseDynamicWeight = true; }
		}
	}

	void THeuristicsHandler::CompleteClusterPreparation()
	{
		TotalStaticWeight = 0;
		for (const UPCGExHeuristicOperation* Op : Operations) { TotalStaticWeight += Op->WeightFactor; }
	}

	FLocalFeedbackHandler* THeuristicsHandler::MakeLocalFeedbackHandler(const PCGExCluster::FCluster* InCluster)
	{
		if (!LocalFeedbackFactories.IsEmpty()) { return nullptr; }
		FLocalFeedbackHandler* NewLocalFeedbackHandler = new FLocalFeedbackHandler();

		for (const UPCGExHeuristicsFactoryBase* Factory : LocalFeedbackFactories)
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

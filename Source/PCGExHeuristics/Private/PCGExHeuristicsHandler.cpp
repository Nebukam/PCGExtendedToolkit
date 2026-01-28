// Copyright 2026 Timothé Lapetite and contributors
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
#pragma region FLocalFeedbackHandler

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

	void FLocalFeedbackHandler::ResetFeedback()
	{
		for (const TSharedPtr<FPCGExHeuristicFeedback>& Feedback : Feedbacks) { Feedback->ResetFeedback(); }
	}

#pragma endregion

#pragma region FHandler

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
		CategorizedOps.Reset();

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			TotalStaticWeight += Op->WeightFactor;

			// Categorize operation for fast-path optimizations
			switch (Op->GetCategory())
			{
			case EPCGExHeuristicCategory::FullyStatic:
				CategorizedOps.FullyStatic.Add(Op);
				CategorizedOps.FullyStaticWeight += Op->WeightFactor;
				break;
			case EPCGExHeuristicCategory::GoalDependent:
				CategorizedOps.GoalDependent.Add(Op);
				CategorizedOps.GoalDependentWeight += Op->WeightFactor;
				break;
			case EPCGExHeuristicCategory::TravelDependent:
				CategorizedOps.TravelDependent.Add(Op);
				CategorizedOps.TravelDependentWeight += Op->WeightFactor;
				CategorizedOps.bHasTravelDependent = true;
				break;
			case EPCGExHeuristicCategory::Feedback:
				// Feedback operations are already in Feedbacks array
				break;
			}
		}
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

	TSharedPtr<FLocalFeedbackHandler> FHandler::AcquireLocalFeedbackHandler(const TSharedPtr<const PCGExClusters::FCluster>& InCluster)
	{
		if (LocalFeedbackFactories.IsEmpty()) { return nullptr; }

		{
			FScopeLock Lock(&PoolLock);
			if (!LocalFeedbackHandlerPool.IsEmpty())
			{
				TSharedPtr<FLocalFeedbackHandler> Handler = LocalFeedbackHandlerPool.Pop();
				Handler->ResetFeedback();
				return Handler;
			}
		}

		// Pool is empty, create new handler
		return MakeLocalFeedbackHandler(InCluster);
	}

	void FHandler::ReleaseLocalFeedbackHandler(const TSharedPtr<FLocalFeedbackHandler>& Handler)
	{
		if (!Handler) { return; }

		FScopeLock Lock(&PoolLock);
		LocalFeedbackHandlerPool.Add(Handler);
	}

	TSharedPtr<FHandler> FHandler::CreateHandler(
		EPCGExHeuristicScoreMode ScoreMode,
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InVtxDataCache,
		const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache,
		const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories)
	{
		switch (ScoreMode)
		{
		case EPCGExHeuristicScoreMode::WeightedAverage:
			return MakeShared<FHandlerWeightedAverage>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		case EPCGExHeuristicScoreMode::GeometricMean:
			return MakeShared<FHandlerGeometricMean>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		case EPCGExHeuristicScoreMode::WeightedSum:
			return MakeShared<FHandlerWeightedSum>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		case EPCGExHeuristicScoreMode::HarmonicMean:
			return MakeShared<FHandlerHarmonicMean>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		case EPCGExHeuristicScoreMode::Min:
			return MakeShared<FHandlerMin>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		case EPCGExHeuristicScoreMode::Max:
			return MakeShared<FHandlerMax>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		default:
			return MakeShared<FHandlerWeightedAverage>(InContext, InVtxDataCache, InEdgeDataCache, InFactories);
		}
	}

#pragma endregion

#pragma region FHandlerWeightedAverage

	double FHandlerWeightedAverage::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		double GScore = 0;
		double TotalWeight = TotalStaticWeight;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { GScore += Op->GetGlobalScore(From, Seed, Goal); }
		if (LocalFeedback)
		{
			GScore += LocalFeedback->GetGlobalScore(From, Seed, Goal);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return TotalWeight > 0 ? GScore / TotalWeight : 0;
	}

	double FHandlerWeightedAverage::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double EScore = 0;
		double TotalWeight = TotalStaticWeight;

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }

			if (LocalFeedback)
			{
				EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
				TotalWeight += LocalFeedback->TotalStaticWeight;
			}

			return TotalWeight > 0 ? EScore / TotalWeight : 0;
		}

		// Dynamic weight path: apply per-edge custom weight multipliers
		TotalWeight = 0;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier;
			TotalWeight += Op->WeightFactor * Multiplier;
		}

		if (LocalFeedback)
		{
			EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return TotalWeight > 0 ? EScore / TotalWeight : 0;
	}

#pragma endregion

#pragma region FHandlerGeometricMean

	double FHandlerGeometricMean::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		// Geometric mean: exp(sum(weight * log(score)) / sum(weight))
		// To avoid log(0), we clamp scores to a small minimum
		constexpr double MinScore = 1e-10;

		double WeightedLogSum = 0;
		double TotalWeight = TotalStaticWeight;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Score = FMath::Max(MinScore, Op->GetGlobalScore(From, Seed, Goal));
			// Score already includes WeightFactor via ReferenceWeight, so we use WeightFactor for the exponent
			WeightedLogSum += Op->WeightFactor * FMath::Loge(Score / Op->WeightFactor); // Normalize out the weight from score first
		}

		if (LocalFeedback)
		{
			const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetGlobalScore(From, Seed, Goal));
			WeightedLogSum += LocalFeedback->TotalStaticWeight * FMath::Loge(FeedbackScore / LocalFeedback->TotalStaticWeight);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return TotalWeight > 0 ? FMath::Exp(WeightedLogSum / TotalWeight) : 0;
	}

	double FHandlerGeometricMean::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		constexpr double MinScore = 1e-10;

		double WeightedLogSum = 0;
		double TotalWeight = TotalStaticWeight;

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
			{
				const double Score = FMath::Max(MinScore, Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
				WeightedLogSum += Op->WeightFactor * FMath::Loge(Score / Op->WeightFactor);
			}

			if (LocalFeedback)
			{
				const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
				WeightedLogSum += LocalFeedback->TotalStaticWeight * FMath::Loge(FeedbackScore / LocalFeedback->TotalStaticWeight);
				TotalWeight += LocalFeedback->TotalStaticWeight;
			}

			return TotalWeight > 0 ? FMath::Exp(WeightedLogSum / TotalWeight) : 0;
		}

		// Dynamic weight path
		TotalWeight = 0;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			const double EffectiveWeight = Op->WeightFactor * Multiplier;
			const double Score = FMath::Max(MinScore, Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier);
			WeightedLogSum += EffectiveWeight * FMath::Loge(Score / EffectiveWeight);
			TotalWeight += EffectiveWeight;
		}

		if (LocalFeedback)
		{
			const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
			WeightedLogSum += LocalFeedback->TotalStaticWeight * FMath::Loge(FeedbackScore / LocalFeedback->TotalStaticWeight);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return TotalWeight > 0 ? FMath::Exp(WeightedLogSum / TotalWeight) : 0;
	}

#pragma endregion

#pragma region FHandlerWeightedSum

	double FHandlerWeightedSum::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		double GScore = 0;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { GScore += Op->GetGlobalScore(From, Seed, Goal); }
		if (LocalFeedback)
		{
			GScore += LocalFeedback->GetGlobalScore(From, Seed, Goal);
		}

		// No normalization - weights directly scale contribution
		return GScore;
	}

	double FHandlerWeightedSum::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double EScore = 0;

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }

			if (LocalFeedback)
			{
				EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
			}

			return EScore;
		}

		// Dynamic weight path: apply per-edge custom weight multipliers
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier;
		}

		if (LocalFeedback)
		{
			EScore += LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
		}

		return EScore;
	}

#pragma endregion

#pragma region FHandlerHarmonicMean

	double FHandlerHarmonicMean::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		// Harmonic mean: sum(weight) / sum(weight/score)
		// Heavily emphasizes low scores - a single low score dominates the result
		constexpr double MinScore = 1e-10;

		double WeightedInverseSum = 0;
		double TotalWeight = TotalStaticWeight;

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Score = FMath::Max(MinScore, Op->GetGlobalScore(From, Seed, Goal));
			// Normalize score by weight first, then compute inverse
			WeightedInverseSum += Op->WeightFactor / (Score / Op->WeightFactor);
		}

		if (LocalFeedback)
		{
			const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetGlobalScore(From, Seed, Goal));
			WeightedInverseSum += LocalFeedback->TotalStaticWeight / (FeedbackScore / LocalFeedback->TotalStaticWeight);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return WeightedInverseSum > 0 ? TotalWeight / WeightedInverseSum : 0;
	}

	double FHandlerHarmonicMean::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		constexpr double MinScore = 1e-10;

		double WeightedInverseSum = 0;
		double TotalWeight = TotalStaticWeight;

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
			{
				const double Score = FMath::Max(MinScore, Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
				WeightedInverseSum += Op->WeightFactor / (Score / Op->WeightFactor);
			}

			if (LocalFeedback)
			{
				const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
				WeightedInverseSum += LocalFeedback->TotalStaticWeight / (FeedbackScore / LocalFeedback->TotalStaticWeight);
				TotalWeight += LocalFeedback->TotalStaticWeight;
			}

			return WeightedInverseSum > 0 ? TotalWeight / WeightedInverseSum : 0;
		}

		// Dynamic weight path
		TotalWeight = 0;
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			const double EffectiveWeight = Op->WeightFactor * Multiplier;
			const double Score = FMath::Max(MinScore, Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier);
			WeightedInverseSum += EffectiveWeight / (Score / EffectiveWeight);
			TotalWeight += EffectiveWeight;
		}

		if (LocalFeedback)
		{
			const double FeedbackScore = FMath::Max(MinScore, LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack));
			WeightedInverseSum += LocalFeedback->TotalStaticWeight / (FeedbackScore / LocalFeedback->TotalStaticWeight);
			TotalWeight += LocalFeedback->TotalStaticWeight;
		}

		return WeightedInverseSum > 0 ? TotalWeight / WeightedInverseSum : 0;
	}

#pragma endregion

#pragma region FHandlerMin

	double FHandlerMin::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		// Min: returns the lowest score (normalized by weight)
		// Most permissive - any heuristic can allow passage
		double MinScore = TNumericLimits<double>::Max();

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Score = Op->GetGlobalScore(From, Seed, Goal) / Op->WeightFactor; // Normalize
			MinScore = FMath::Min(MinScore, Score);
		}

		if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
		{
			const double FeedbackScore = LocalFeedback->GetGlobalScore(From, Seed, Goal) / LocalFeedback->TotalStaticWeight;
			MinScore = FMath::Min(MinScore, FeedbackScore);
		}

		return MinScore == TNumericLimits<double>::Max() ? 0 : MinScore;
	}

	double FHandlerMin::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double MinScore = TNumericLimits<double>::Max();

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
			{
				const double Score = Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / Op->WeightFactor;
				MinScore = FMath::Min(MinScore, Score);
			}

			if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
			{
				const double FeedbackScore = LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / LocalFeedback->TotalStaticWeight;
				MinScore = FMath::Min(MinScore, FeedbackScore);
			}

			return MinScore == TNumericLimits<double>::Max() ? 0 : MinScore;
		}

		// Dynamic weight path
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			const double EffectiveWeight = Op->WeightFactor * Multiplier;
			if (EffectiveWeight > 0)
			{
				const double Score = (Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier) / EffectiveWeight;
				MinScore = FMath::Min(MinScore, Score);
			}
		}

		if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
		{
			const double FeedbackScore = LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / LocalFeedback->TotalStaticWeight;
			MinScore = FMath::Min(MinScore, FeedbackScore);
		}

		return MinScore == TNumericLimits<double>::Max() ? 0 : MinScore;
	}

#pragma endregion

#pragma region FHandlerMax

	double FHandlerMax::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback) const
	{
		// Max: returns the highest score (normalized by weight)
		// Most restrictive - any heuristic can block passage
		double MaxScore = TNumericLimits<double>::Lowest();

		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Score = Op->GetGlobalScore(From, Seed, Goal) / Op->WeightFactor; // Normalize
			MaxScore = FMath::Max(MaxScore, Score);
		}

		if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
		{
			const double FeedbackScore = LocalFeedback->GetGlobalScore(From, Seed, Goal) / LocalFeedback->TotalStaticWeight;
			MaxScore = FMath::Max(MaxScore, FeedbackScore);
		}

		return MaxScore == TNumericLimits<double>::Lowest() ? 0 : MaxScore;
	}

	double FHandlerMax::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback, const TSharedPtr<PCGEx::FHashLookup>& TravelStack) const
	{
		double MaxScore = TNumericLimits<double>::Lowest();

		if (!bUseDynamicWeight)
		{
			for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
			{
				const double Score = Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / Op->WeightFactor;
				MaxScore = FMath::Max(MaxScore, Score);
			}

			if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
			{
				const double FeedbackScore = LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / LocalFeedback->TotalStaticWeight;
				MaxScore = FMath::Max(MaxScore, FeedbackScore);
			}

			return MaxScore == TNumericLimits<double>::Lowest() ? 0 : MaxScore;
		}

		// Dynamic weight path
		for (const TSharedPtr<FPCGExHeuristicOperation>& Op : Operations)
		{
			const double Multiplier = Op->GetCustomWeightMultiplier(To.Index, Edge.PointIndex);
			const double EffectiveWeight = Op->WeightFactor * Multiplier;
			if (EffectiveWeight > 0)
			{
				const double Score = (Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) * Multiplier) / EffectiveWeight;
				MaxScore = FMath::Max(MaxScore, Score);
			}
		}

		if (LocalFeedback && LocalFeedback->TotalStaticWeight > 0)
		{
			const double FeedbackScore = LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack) / LocalFeedback->TotalStaticWeight;
			MaxScore = FMath::Max(MaxScore, FeedbackScore);
		}

		return MaxScore == TNumericLimits<double>::Lowest() ? 0 : MaxScore;
	}

#pragma endregion
}

#undef PCGEX_INIT_HEURISTIC_OPERATION

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Heuristics/PCGExHeuristicFeedback.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExManagedObjects.h"

double FPCGExHeuristicFeedback::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const
{
	FReadScopeLock ReadScopeLock(FeedbackLock);

	const uint32* N = NodeFeedbackNum.Find(From.Index);
	return N ? GetScoreInternal(NodeScale) * *N : GetScoreInternal(0);
}

double FPCGExHeuristicFeedback::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	FReadScopeLock ReadScopeLock(FeedbackLock);

	const uint32* N = NodeFeedbackNum.Find(To.Index);
	const uint32* E = EdgeFeedbackNum.Find(Edge.Index);

	if (bBinary)
	{
		return N || E ? GetScoreInternal(1) : GetScoreInternal(0);
	}

	const double NW = N ? GetScoreInternal(NodeScale) * *N : GetScoreInternal(0);
	const double EW = E ? GetScoreInternal(EdgeScale) * *E : GetScoreInternal(0);

	return (NW + EW);
}

void FPCGExHeuristicFeedback::FeedbackPointScore(const PCGExClusters::FNode& Node)
{
	FWriteScopeLock WriteScopeLock(FeedbackLock);

	uint32& N = NodeFeedbackNum.FindOrAdd(Node.Index, 0);
	N++;

	if (bBleed)
	{
		for (const PCGExGraphs::FLink Lk : Node.Links)
		{
			uint32& E = EdgeFeedbackNum.FindOrAdd(Lk.Edge, 0);
			E++;
		}
	}
}

void FPCGExHeuristicFeedback::FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge)
{
	FWriteScopeLock WriteScopeLock(FeedbackLock);

	uint32& N = NodeFeedbackNum.FindOrAdd(Node.Index, 0);
	N++;

	if (bBleed)
	{
		for (const PCGExGraphs::FLink Lk : Node.Links)
		{
			uint32& E = EdgeFeedbackNum.FindOrAdd(Lk.Edge, 0);
			E++;
		}
	}
	else
	{
		uint32& E = EdgeFeedbackNum.FindOrAdd(Edge.Index, 0);
		E++;
	}
}

TSharedPtr<FPCGExHeuristicOperation> UPCGExHeuristicsFactoryFeedback::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(HeuristicFeedback)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->NodeScale = Config.VisitedPointsWeightFactor;
	NewOperation->EdgeScale = Config.VisitedEdgesWeightFactor;
	NewOperation->bBleed = Config.bAffectAllConnectedEdges;
	NewOperation->bBinary = Config.bBinary;
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Feedback, {})

UPCGExFactoryData* UPCGExHeuristicFeedbackProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryFeedback* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryFeedback>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicFeedbackProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX")) + TEXT(" @ ") + FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif

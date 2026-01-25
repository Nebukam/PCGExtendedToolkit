// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExClusterCommon.h"
#include "UObject/Object.h"
#include "Factories/PCGExOperation.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Utils/PCGExCurveLookup.h"

namespace PCGEx
{
	class FHashLookup;
}

namespace PCGExGraphs
{
	struct FEdge;
}

namespace PCGExClusters
{
	struct FNode;
	class FCluster;
}

/** Categories for heuristic operations to enable fast-path optimizations */
enum class EPCGExHeuristicCategory : uint8
{
	/** Stateless, cluster-level pre-computable (e.g., Distance edge scores, Attribute) */
	FullyStatic,
	/** Goal-dependent but stateless within a query (e.g., Azimuth, Distance global scores) */
	GoalDependent,
	/** Requires TravelStack for path history (e.g., Inertia, Steepness with accumulation) */
	TravelDependent,
	/** Feedback tracking operations */
	Feedback
};

/**
 *
 */
class PCGEXHEURISTICS_API FPCGExHeuristicOperation : public FPCGExOperation
{
public:
	bool bInvert = false;
	double ReferenceWeight = 1;
	double WeightFactor = 1;
	bool bUseLocalWeightMultiplier = false;
	FVector UVWSeed = FVector::OneVector * -1;
	FVector UVWGoal = FVector::OneVector;
	EPCGExClusterElement LocalWeightMultiplierSource = EPCGExClusterElement::Vtx;
	FPCGAttributePropertyInputSelector WeightMultiplierAttribute;

	PCGExFloatLUT ScoreCurve = nullptr;

	bool bHasCustomLocalWeightMultiplier = false;

	/** Returns the category of this heuristic for optimization purposes */
	virtual EPCGExHeuristicCategory GetCategory() const { return EPCGExHeuristicCategory::GoalDependent; }

	virtual void PrepareForCluster(const TSharedPtr<const PCGExClusters::FCluster>& InCluster);

	virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const;


	virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack = nullptr) const;


	double GetCustomWeightMultiplier(const int32 PointIndex, const int32 EdgeIndex) const;

	FORCEINLINE FVector GetSeedUVW() const { return UVWSeed; }
	FORCEINLINE FVector GetGoalUVW() const { return UVWGoal; }

	const PCGExClusters::FNode* GetRoamingSeed() const;
	const PCGExClusters::FNode* GetRoamingGoal() const;

protected:
	TSharedPtr<const PCGExClusters::FCluster> Cluster;
	TArray<double> LocalWeightMultiplier;

	virtual double GetScoreInternal(const double InTime) const;
};

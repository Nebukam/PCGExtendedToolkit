// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "PCGExOperation.h"
#include "Details/PCGExDetailsCluster.h"
#include "Metadata/PCGAttributePropertySelector.h"

namespace PCGEx
{
	class FHashLookup;
}

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExCluster
{
	struct FNode;
	class FCluster;
}

/**
 * 
 */
class PCGEXTENDEDTOOLKIT_API FPCGExHeuristicOperation : public FPCGExOperation
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

	const FRichCurve* ScoreCurve = nullptr;

	bool bHasCustomLocalWeightMultiplier = false;

	virtual void PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster);

	virtual double GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const;


	virtual double GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack = nullptr) const;


	double GetCustomWeightMultiplier(const int32 PointIndex, const int32 EdgeIndex) const;

	FORCEINLINE FVector GetSeedUVW() const { return UVWSeed; }
	FORCEINLINE FVector GetGoalUVW() const { return UVWGoal; }

	const PCGExCluster::FNode* GetRoamingSeed() const;
	const PCGExCluster::FNode* GetRoamingGoal() const;

protected:
	TSharedPtr<const PCGExCluster::FCluster> Cluster;
	TArray<double> LocalWeightMultiplier;

	virtual double GetScoreInternal(const double InTime) const;
};

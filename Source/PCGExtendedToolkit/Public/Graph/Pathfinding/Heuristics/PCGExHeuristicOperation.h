// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> ScoreCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	double ReferenceWeight = 100;

	virtual void PrepareForData(PCGExCluster::FCluster* InCluster);

	virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const;

	virtual void Cleanup() override;

protected:
	PCGExCluster::FCluster* Cluster = nullptr;
	TObjectPtr<UCurveFloat> ScoreCurveObj;
};

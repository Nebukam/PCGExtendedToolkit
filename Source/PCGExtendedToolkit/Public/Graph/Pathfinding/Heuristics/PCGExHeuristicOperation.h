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
	double ReferenceWeight = 100;
	
	virtual void PrepareForData(PCGExCluster::FCluster* InCluster);
	virtual double ComputeScore(
		const PCGExCluster::FScoredNode* From,
		const PCGExCluster::FNode& To,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const PCGExGraph::FIndexedEdge& Edge) const;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const;
	virtual int32 GetQueueingIndex(const TArray<PCGExCluster::FScoredNode*>& InList, const double InScore, const int32 A) const;

	virtual void ScoredInsert(TArray<PCGExCluster::FScoredNode*>& InList, PCGExCluster::FScoredNode* Node) const;
	
	virtual void Cleanup() override;
	
protected:
	PCGExCluster::FCluster* Cluster = nullptr;
	
};

// Copyright Timothé Lapetite 2023
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
	virtual void PrepareForData(const PCGExCluster::FCluster* InCluster);
	virtual double ComputeScore(
		const PCGExCluster::FScoredVertex* From,
		const PCGExCluster::FVertex& To,
		const PCGExCluster::FVertex& Seed,
		const PCGExCluster::FVertex& Goal,
		const PCGExGraph::FIndexedEdge& Edge) const;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const;
	virtual int32 GetQueueingIndex(const TArray<PCGExCluster::FScoredVertex*>& InVertices, const double InScore) const;
	double GetScale() const { return IsBetterScore(-1, 1) ? 1 : -1; }
};

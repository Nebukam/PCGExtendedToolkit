// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExMesh.h"
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
	virtual void PrepareForData(const PCGExMesh::FMesh* InMesh);
	virtual double ComputeScore(
		const PCGExMesh::FScoredVertex* From,
		const PCGExMesh::FVertex& To,
		const PCGExMesh::FVertex& Seed,
		const PCGExMesh::FVertex& Goal) const;
	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const;
	virtual int32 GetQueueingIndex(const TArray<PCGExMesh::FScoredVertex*>& InVertices, const double InScore) const;
};

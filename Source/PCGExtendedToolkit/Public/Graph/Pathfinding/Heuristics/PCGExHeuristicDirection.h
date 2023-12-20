// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExMesh.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicDirection.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Direction")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDirection : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual double ComputeScore(
		const PCGExMesh::FScoredVertex* From,
		const PCGExMesh::FVertex& To,
		const PCGExMesh::FVertex& Seed,
		const PCGExMesh::FVertex& Goal) const override;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const override;
};

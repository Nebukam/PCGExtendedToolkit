﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExMesh.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "PCGExHeuristicModifiersOnly.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Modifiers Only")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicModifiersOnly : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual double ComputeScore(
		const PCGExMesh::FScoredVertex* From,
		const PCGExMesh::FVertex& To,
		const PCGExMesh::FVertex& Seed,
		const PCGExMesh::FVertex& Goal,
		const PCGExMesh::FIndexedEdge& Edge) const override;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const override;

	/** How to interpret the data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-2))
	EPCGExHeuristicScoreMode BaseInterpretation = EPCGExHeuristicScoreMode::HigherIsBetter;
};

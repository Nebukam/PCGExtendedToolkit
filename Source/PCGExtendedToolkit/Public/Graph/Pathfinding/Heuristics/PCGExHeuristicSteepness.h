// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicDistance.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicSteepness.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Steepness")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicSteepness : public UPCGExHeuristicDistance
{
	GENERATED_BODY()

public:
	/** Invert the heuristics so it looks away from the target instead of towards it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** Vector pointing in the "up" direction. Mirrored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UpVector = FVector::UpVector;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> ScoreCurve;

	/** Adds result from Shortest Distance heuristics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddDistanceHeuristic = true;

	/** Distance weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bAddDistanceHeuristic", ClampMin="0.001"))
	double DistanceWeight = 10;

	virtual void PrepareForData(PCGExCluster::FCluster* InCluster) override;

	virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

protected:
	FVector UpwardVector = FVector::UpVector;
	TObjectPtr<UCurveFloat> ScoreCurveObj;

	void ApplyOverrides() override;
	
};

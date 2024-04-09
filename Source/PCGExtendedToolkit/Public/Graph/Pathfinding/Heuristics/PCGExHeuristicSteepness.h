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

	/** Curve the steepness value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> SteepnessScoreCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::SteepnessWeightCurve);

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
	TObjectPtr<UCurveFloat> SteepnessScoreCurveObj;
	double ReverseWeight = 1.0;

	double GetDot(const FVector& From, const FVector& To) const;

	virtual void ApplyOverrides() override;
};

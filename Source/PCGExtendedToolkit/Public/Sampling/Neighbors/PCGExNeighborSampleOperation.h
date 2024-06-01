// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExNeighborSampleFactoryProvider.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExNeighborSampleOperation.generated.h"

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	PCGExDataBlending::FMetadataBlender* Blender = nullptr;
	PCGExDataBlending::FDataBlendingOperationBase* BlendOp = nullptr;
	
	EPCGExRangeType RangeType = EPCGExRangeType::FullRange;
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Index;
	int32 MaxDepth = 1;
	double MaxDistance = 300;
	double FixedBlend = 1;
	TObjectPtr<UCurveFloat> WeightCurveObj = nullptr;
	EPCGExGraphValueSource NeighborSource = EPCGExGraphValueSource::Point;
	FName SourceAttribute;
	bool bOutputToNewAttribute = false;
	FName TargetAttribute;
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster);

	FORCEINLINE virtual void ProcessNodeForPoints(
		const PCGEx::FPointRef& Target,
		const PCGExCluster::FNode& Node) const;
	
	FORCEINLINE virtual void ProcessNodeForEdges(
		const PCGEx::FPointRef& Target,
		const PCGExCluster::FNode& Node) const;

	virtual void Cleanup() override;

protected:
	PCGExCluster::FCluster* Cluster = nullptr;

	FORCEINLINE virtual double SampleCurve(const double InTime) const;
	FORCEINLINE void GrabNeighbors(
		const PCGExCluster::FNode& From,
		TArray<int32>& OutNeighbors,
		const TSet<int32>& Visited) const;
	
	FORCEINLINE void GrabNeighborsEdges(
		const PCGExCluster::FNode& From,
		TArray<int32>& OutNeighbors,
		TArray<int32>& OutNeighborsEdges,
		const TSet<int32>& Visited,
		const TSet<int32>& VisitedEdges) const;
};

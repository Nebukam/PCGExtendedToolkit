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

	PCGExCluster::FNodeStateHandler* PointFilters = nullptr;
	PCGExCluster::FNodeStateHandler* UsableValueFilters = nullptr;
	
	PCGExDataBlending::FMetadataBlender* Blender = nullptr;
	TArray<PCGExDataBlending::FDataBlendingOperationBase*> BlendOps;
	
	EPCGExRangeType RangeType = EPCGExRangeType::FullRange;
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Index;
	int32 MaxDepth = 1;
	double MaxDistance = 300;
	double FixedBlend = 1;
	TObjectPtr<UCurveFloat> WeightCurveObj = nullptr;
	EPCGExGraphValueSource NeighborSource = EPCGExGraphValueSource::Point;
	TSet<FName> SourceAttributes;
	//bool bOutputToNewAttribute = false;
	//FName TargetAttribute;
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster);

	FORCEINLINE virtual void ProcessNodeForPoints(
		const int32 InNodeIndex) const;
	
	FORCEINLINE virtual void ProcessNodeForEdges(
		const int32 InNodeIndex) const;

	virtual void Cleanup() override;

protected:
	PCGExCluster::FCluster* Cluster = nullptr;

	FORCEINLINE virtual double SampleCurve(const double InTime) const;
};

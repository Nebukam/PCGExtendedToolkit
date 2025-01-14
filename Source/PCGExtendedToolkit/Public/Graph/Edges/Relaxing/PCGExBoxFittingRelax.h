// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingRelaxBase.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExBoxFittingRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Box Fitting")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoxFittingRelax : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExBoxFittingRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	virtual bool PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InCluster)) { return false; }
		return true;
	}

	virtual void Step2(const PCGExCluster::FNode& Node) override
	{
		const FTransform& CurrentTr = *(ReadBuffer->GetData() + Node.Index);
		const FBox CurrentBox = PrimaryDataFacade->Source->GetInPoint(Node.PointIndex).GetLocalBounds().TransformBy(CurrentTr);
		const FVector& CurrentPos = CurrentTr.GetLocation();

		// Apply repulsion forces between all pairs of nodes

		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
		{
			const FTransform& OtherTr = *(ReadBuffer->GetData() + OtherNodeIndex);
			const PCGExCluster::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

			// Transform boxes to world space
			const FBox OtherBox = PrimaryDataFacade->Source->GetInPoint(OtherNode->PointIndex).GetLocalBounds().TransformBy(OtherTr);

			// Check for overlap
			if (!CurrentBox.Intersect(OtherBox)) { continue; }

			// Calculate overlap resolution force
			// TODO : Test with repulsion based on overlap size
			FVector Delta = OtherPos - CurrentPos;
			const double Distance = Delta.Size();

			if (Distance <= KINDA_SMALL_NUMBER) { continue; }

			// Overlap resolution
			FVector OverlapSize = CurrentBox.GetExtent() + OtherBox.GetExtent() - PCGExMath::Abs(Delta);

			ApplyForces(OtherNode->Index, Node.Index, (RepulsionConstant * OverlapSize * (Delta / Distance)) * Precision);
		}
	}
};

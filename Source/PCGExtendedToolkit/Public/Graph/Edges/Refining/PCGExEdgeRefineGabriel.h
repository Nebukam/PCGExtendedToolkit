// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExEdgeRefineGabriel.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Gabriel"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineGabriel : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return true; }
	virtual bool RequiresNodeOctree() override { return true; }

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineGabriel* TypedOther = Cast<UPCGExEdgeRefineGabriel>(Other))
		{
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }

	virtual void ProcessEdge(PCGExGraph::FEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const FVector From = Cluster->GetStartPos(Edge);
		const FVector To = Cluster->GetEndPos(Edge);

		const FVector Center = FMath::Lerp(From, To, 0.5);
		const double SqrDist = FVector::DistSquared(Center, From);

		Cluster->NodeOctree->FindFirstElementWithBoundsTest(
			FBoxCenterAndExtent(Center, FVector(FMath::Sqrt(SqrDist))), [&](const PCGEx::FIndexedItem& Item)
			{
				if (FVector::DistSquared(Center, Cluster->GetPos(Item.Index)) < SqrDist)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
					return false;
				}
				return true;
			});
	}
};

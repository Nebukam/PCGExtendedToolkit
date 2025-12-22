// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineGabriel.generated.h"

/**
 * 
 */
class FPCGExEdgeRefineGabriel : public FPCGExEdgeRefineOperation
{
public:
	virtual void PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHandler>& InHeuristics) override
	{
		FPCGExEdgeRefineOperation::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 1 : 0;
	}

	virtual void ProcessEdge(PCGExGraphs::FEdge& Edge) override
	{
		FPCGExEdgeRefineOperation::ProcessEdge(Edge);

		const FVector From = Cluster->GetStartPos(Edge);
		const FVector To = Cluster->GetEndPos(Edge);

		const FVector Center = FMath::Lerp(From, To, 0.5);
		const double SqrDist = FVector::DistSquared(Center, From);

		Cluster->NodeOctree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Center, FVector(FMath::Sqrt(SqrDist))), [&](const PCGExOctree::FItem& Item)
		{
			if (FVector::DistSquared(Center, Cluster->GetPos(Item.Index)) < SqrDist)
			{
				FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
				return false;
			}
			return true;
		});
	}

	int8 ExchangeValue = 0;
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Gabriel", PCGExNodeLibraryDoc="clusters/refine-cluster/gabriel"))
class UPCGExEdgeRefineGabriel : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return !bInvert; }
	virtual bool WantsNodeOctree() const override { return true; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineGabriel* TypedOther = Cast<UPCGExEdgeRefineGabriel>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool WantsIndividualEdgeProcessing() const override { return !bInvert; }

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefineGabriel, { Operation->bInvert = bInvert; })
};

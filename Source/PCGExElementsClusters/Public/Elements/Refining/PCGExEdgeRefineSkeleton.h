// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "Math/PCGExMathAxis.h"
#include "PCGExEdgeRefineSkeleton.generated.h"

/**
 * 
 */
class FPCGExEdgeRefineSkeleton : public FPCGExEdgeRefineOperation
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
		const double Dist = FVector::Dist(From, To);

		if (Beta <= 1)
		{
			// Lune-based condition (Beta-Skeleton for 0 < Beta <= 1)
			const double SqrDist = FMath::Square(Dist / Beta);

			Cluster->NodeOctree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Center, FVector(FMath::Sqrt(SqrDist) + 1)), [&](const PCGExOctree::FItem& Item)
			{
				const FVector& OtherPoint = Cluster->GetPos(Item.Index);
				if (FVector::DistSquared(OtherPoint, From) < SqrDist && FVector::DistSquared(OtherPoint, To) < SqrDist)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
					return false;
				}
				return true;
			});
		}
		else
		{
			const FVector Normal = PCGExMath::GetNormalUp(From, To, FVector::UpVector) * (Dist * Beta);
			const double SqrDist = FMath::Square(Dist);

			const FVector C1 = Center + Normal;
			const FVector C2 = Center - Normal;

			Cluster->NodeOctree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(Center, FVector(FMath::Sqrt(SqrDist) + 1)), [&](const PCGExOctree::FItem& Item)
			{
				const FVector& OtherPoint = Cluster->GetPos(Item.Index);
				if (FVector::DistSquared(OtherPoint, C1) < SqrDist || FVector::DistSquared(OtherPoint, C2) < SqrDist)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
					return false;
				}
				return true;
			});
		}
	}

	int8 ExchangeValue = 0;

	double Beta = 1;
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : β Skeleton", PCGExNodeLibraryDoc="clusters/refine-cluster/v-skeleton"))
class UPCGExEdgeRefineSkeleton : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return !bInvert; }
	virtual bool WantsNodeOctree() const override { return true; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineSkeleton* TypedOther = Cast<UPCGExEdgeRefineSkeleton>(Other))
		{
			Beta = TypedOther->Beta;
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool WantsIndividualEdgeProcessing() const override { return true; }

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Beta = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefineSkeleton, { Operation->Beta = Beta; Operation->bInvert = bInvert; })
};

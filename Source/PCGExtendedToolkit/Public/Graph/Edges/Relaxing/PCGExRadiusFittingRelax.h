// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBoxFittingRelax.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExRadiusFittingRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Radius Fitting")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRadiusFittingRelax : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExRadiusFittingRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		RadiusAttribute.Update(TEXT("$Extents.Length"));
	}

	/** Vtx radii */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Radius"))
	FPCGAttributePropertyInputSelector RadiusAttribute;

	virtual bool PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InCluster)) { return false; }

		RadiusBuffer = PrimaryDataFacade->GetBroadcaster<double>(RadiusAttribute);
		if (!RadiusBuffer)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Invalid Radius attribute: \"{0}\"."), FText::FromName(RadiusAttribute.GetName())));
			return false;
		}

		return true;
	}

	virtual void Step2(const PCGExCluster::FNode& Node) override
	{
		const FVector& CurrentPos = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		const double& CurrentRadius = RadiusBuffer->Read(Node.PointIndex);

		// Apply repulsion forces between all pairs of nodes

		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
		{
			const PCGExCluster::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

			FVector Delta = OtherPos - CurrentPos;
			const double Distance = Delta.Size();
			const double Overlap = (CurrentRadius + RadiusBuffer->Read(OtherNode->PointIndex)) - Distance;

			if (Overlap <= 0 || Distance <= KINDA_SMALL_NUMBER) { continue; }

			ApplyForces(
				OtherNode->Index, Node.Index,
				(RepulsionConstant * (Overlap / FMath::Square(Distance)) * (Delta / Distance)) * Precision);
		}
	}

protected:
	TSharedPtr<PCGExData::TBuffer<double>> RadiusBuffer;
};

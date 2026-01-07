// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingRelaxBase.h"
#include "PCGExRelaxClusterOperation.h"
#include "Types/PCGExTypes.h"

#include "PCGExBoxFittingRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Box Fitting", PCGExNodeLibraryDoc="clusters/relax-cluster/box-fitting"))
class UPCGExBoxFittingRelax : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExBoxFittingRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	/** A padding value added to the box bounds to attempt to reduce overlap or add more spacing between boxes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Padding = 10;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
		PCGExArrayHelpers::InitArray(BoxBuffer, Cluster->Nodes->Num());
		return true;
	}

	virtual EPCGExClusterElement PrepareNextStep(const int32 InStep) override
	{
		EPCGExClusterElement Source = Super::PrepareNextStep(InStep); // Super does the buffer swap, needs to happen first
		if (InStep == 0)
		{
			const int32 NumNodes = Cluster->Nodes->Num();
			const UPCGBasePointData* InPointData = PrimaryDataFacade->GetIn();

			for (int i = 0; i < NumNodes; i++)
			{
				BoxBuffer[i] = InPointData->GetLocalBounds(Cluster->GetNodePointIndex(i)).ExpandBy(Padding).TransformBy(*(ReadBuffer->GetData() + i));
			}
		}
		return Source;
	}

	virtual void Step2(const PCGExClusters::FNode& Node) override
	{
		const FTransform& CurrentTr = *(ReadBuffer->GetData() + Node.Index);
		const FBox CurrentBox = BoxBuffer[Node.Index];
		const FVector& CurrentPos = CurrentTr.GetLocation();

		// Apply repulsion forces between all pairs of nodes
		const int32 NumNodes = Cluster->Nodes->Num();
		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < NumNodes; OtherNodeIndex++)
		{
			const FTransform& OtherTr = *(ReadBuffer->GetData() + OtherNodeIndex);
			const PCGExClusters::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

			// Transform boxes to world space
			const FBox OtherBox = BoxBuffer[OtherNodeIndex];

			// Check for overlap
			if (!CurrentBox.Intersect(OtherBox)) { continue; }

			// Calculate overlap resolution force
			// TODO : Test with repulsion based on overlap size
			FVector Delta = OtherPos - CurrentPos;
			const double Distance = Delta.Size();

			if (Distance <= KINDA_SMALL_NUMBER) { continue; }

			// Overlap resolution
			FVector OverlapSize = CurrentBox.GetExtent() + OtherBox.GetExtent() - PCGExTypes::Abs(Delta);

			AddDelta(OtherNode->Index, Node.Index, (RepulsionConstant * OverlapSize * (Delta / Distance)));
		}
	}

protected:
	TArray<FBox> BoxBuffer;
};

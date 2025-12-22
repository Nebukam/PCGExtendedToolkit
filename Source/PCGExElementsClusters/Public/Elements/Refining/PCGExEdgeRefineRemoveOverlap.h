// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "Math/PCGExMath.h"
#include "PCGExEdgeRefineRemoveOverlap.generated.h"

UENUM()
enum class EPCGExEdgeOverlapPick : uint8
{
	Shortest = 0 UMETA(DisplayName = "Shortest", ToolTip="Keep the shortest edge"),
	Longest  = 1 UMETA(DisplayName = "Longest", ToolTip="Keep the longest edge"),
};

/**
 * 
 */
class FPCGExEdgeRemoveOverlap : public FPCGExEdgeRefineOperation
{
public:
	virtual void PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHandler>& InHeuristics) override
	{
		FPCGExEdgeRefineOperation::PrepareForCluster(InCluster, InHeuristics);
		MinDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
		MaxDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
		ToleranceSquared = FMath::Square(Tolerance);
		Cluster->GetBoundedEdges(true); // Let's hope it was cached ^_^
	}

	virtual void ProcessEdge(PCGExGraphs::FEdge& Edge) override
	{
		const double Length = Cluster->GetDistSquared(Edge);

		const FVector A1 = Cluster->GetStartPos(Edge);
		const FVector B1 = Cluster->GetEndPos(Edge);

		auto ProcessOverlap = [&](const PCGExOctree::FItem& Item)
		{
			//if (!Edge.bValid) { return false; }

			const PCGExGraphs::FEdge& OtherEdge = *Cluster->GetEdge(Item.Index);

			if (Edge.Index == OtherEdge.Index || Edge.Start == OtherEdge.Start || Edge.Start == OtherEdge.End || Edge.End == OtherEdge.End || Edge.End == OtherEdge.Start) { return true; }


			if (bUseMinAngle || bUseMaxAngle)
			{
				const double Dot = FMath::Abs(FVector::DotProduct(Cluster->GetEdgeDir(Edge), Cluster->GetEdgeDir(OtherEdge)));
				if (!(Dot >= MaxDot && Dot <= MinDot)) { return true; }
			}

			const double OtherLength = Cluster->GetDistSquared(OtherEdge);

			FVector A;
			FVector B;
			if (Cluster->EdgeDistToEdgeSquared(&Edge, &OtherEdge, A, B) >= ToleranceSquared) { return true; }

			const FVector A2 = Cluster->GetStartPos(OtherEdge);
			const FVector B2 = Cluster->GetEndPos(OtherEdge);

			if (A == A1 || A == B1 || A == A2 || A == B2 || B == A2 || B == B2 || B == A1 || B == B1) { return true; }

			// Overlap!
			if (Keep == EPCGExEdgeOverlapPick::Longest)
			{
				if (OtherLength > Length)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
					return false;
				}
			}
			else
			{
				if (OtherLength < Length)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
					return false;
				}
			}

			return true;
		};

		(void)Cluster->GetEdgeOctree()->FindFirstElementWithBoundsTest((Cluster->BoundedEdges->GetData() + Edge.Index)->Bounds.GetBox(), ProcessOverlap);
	}

	//virtual void Process() override;

	EPCGExEdgeOverlapPick Keep = EPCGExEdgeOverlapPick::Longest;

	double Tolerance = DBL_INTERSECTION_TOLERANCE;
	double ToleranceSquared = DBL_INTERSECTION_TOLERANCE * DBL_INTERSECTION_TOLERANCE;

	bool bUseMinAngle = true;
	double MinAngle = 0;
	double MinDot = 1;

	bool bUseMaxAngle = true;
	double MaxAngle = 90;
	double MaxDot = -1;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Overlap", PCGExNodeLibraryDoc="clusters/refine-cluster/overlap"))
class UPCGExEdgeRemoveOverlap : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool WantsIndividualEdgeProcessing() const override { return true; }
	virtual bool WantsEdgeOctree() const override { return true; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRemoveOverlap* TypedOther = Cast<UPCGExEdgeRemoveOverlap>(Other))
		{
			Keep = TypedOther->Keep;
			Tolerance = TypedOther->Tolerance;
			bUseMinAngle = TypedOther->bUseMinAngle;
			MinAngle = TypedOther->MinAngle;
			bUseMaxAngle = TypedOther->bUseMaxAngle;
			MaxAngle = TypedOther->MaxAngle;
		}
	}

	//virtual void Process() override;

	/** Which edge to keep when doing comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgeOverlapPick Keep = EPCGExEdgeOverlapPick::Longest;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = DBL_INTERSECTION_TOLERANCE;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = true;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MinAngle = 0;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = true;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MaxAngle = 90;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRemoveOverlap, { Operation->Keep = Keep; Operation->Tolerance = Tolerance; Operation->bUseMinAngle = bUseMinAngle; Operation->MinAngle = MinAngle; Operation->bUseMaxAngle = bUseMaxAngle; Operation->MaxAngle = MaxAngle; })
};

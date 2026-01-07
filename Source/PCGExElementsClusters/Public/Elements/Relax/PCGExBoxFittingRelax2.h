// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingRelaxBase.h"
#include "PCGExRelaxClusterOperation.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExBoxFittingRelax2.generated.h"

UENUM()
enum class EPCGExBoxFittingSeparation : uint8
{
	MinimumPenetration = 0 UMETA(DisplayName = "Minimum Penetration", ToolTip="Separate along the axis with minimum overlap"),
	EdgeDirection      = 1 UMETA(DisplayName = "Edge Direction", ToolTip="Prefer separation along connected edge directions"),
	Centroid           = 2 UMETA(DisplayName = "Centroid", ToolTip="Separate directly away from each other's centers"),
};

/**
 * Relaxation using axis-aligned bounding boxes for collision detection.
 * More accurate than radius-based for rectangular or elongated objects.
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Box Fitting v2", PCGExNodeLibraryDoc="clusters/relax-cluster/box-fitting"))
class UPCGExBoxFittingRelax2 : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExBoxFittingRelax2(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		ExtentsAttribute.Update(TEXT("$Extents"));
	}

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override
	{
		Super::RegisterPrimaryBuffersDependencies(InContext, FacadePreloader);
		if (ExtentsInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, ExtentsAttribute); }
	}

	/** How extents are determined */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ExtentsInput = EPCGExInputValueType::Attribute;

	/** Attribute to read extents value from. Expected to be half-size in each axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Extents (Attr)", EditCondition="ExtentsInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ExtentsAttribute;

	/** Constant extents value. Half-size in each axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Extents", EditCondition="ExtentsInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Extents = FVector(50, 50, 50);

	PCGEX_SETTING_VALUE_INLINE(Extents, FVector, ExtentsInput, ExtentsAttribute, Extents)

	/** How to determine separation direction when boxes overlap */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBoxFittingSeparation SeparationMode = EPCGExBoxFittingSeparation::MinimumPenetration;

	/** Additional padding between boxes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Padding = 0;

	/** Whether to consider rotation when computing bounds (more expensive) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUseOrientedBounds = false;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

		ExtentsBuffer = GetValueSettingExtents();
		if (!ExtentsBuffer->Init(PrimaryDataFacade)) { return false; }

		return true;
	}

	virtual void Step2(const PCGExClusters::FNode& Node) override
	{
		const FVector& CurrentPos = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		const FVector& CurrentExtents = ExtentsBuffer->Read(Node.PointIndex) + FVector(Padding);

		// Build current node's bounds
		const FBox CurrentBox(CurrentPos - CurrentExtents, CurrentPos + CurrentExtents);

		// Apply repulsion forces between all pairs of nodes
		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
		{
			const PCGExClusters::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();
			const FVector& OtherExtents = ExtentsBuffer->Read(OtherNode->PointIndex) + FVector(Padding);

			// Build other node's bounds
			const FBox OtherBox(OtherPos - OtherExtents, OtherPos + OtherExtents);

			// Check for overlap
			if (!CurrentBox.Intersect(OtherBox)) { continue; }

			// Calculate overlap in each axis
			const FVector OverlapMin = FVector::Max(CurrentBox.Min, OtherBox.Min);
			const FVector OverlapMax = FVector::Min(CurrentBox.Max, OtherBox.Max);
			const FVector OverlapSize = OverlapMax - OverlapMin;

			if (OverlapSize.X <= 0 || OverlapSize.Y <= 0 || OverlapSize.Z <= 0) { continue; }

			FVector SeparationDir;
			double SeparationMagnitude;

			switch (SeparationMode)
			{
			case EPCGExBoxFittingSeparation::MinimumPenetration:
				{
					// Find axis with minimum penetration
					const int32 MinAxis = (OverlapSize.X <= OverlapSize.Y && OverlapSize.X <= OverlapSize.Z) ? 0 : (OverlapSize.Y <= OverlapSize.Z) ? 1 : 2;

					SeparationDir = FVector::ZeroVector;
					SeparationDir[MinAxis] = (CurrentPos[MinAxis] < OtherPos[MinAxis]) ? -1.0 : 1.0;
					SeparationMagnitude = OverlapSize[MinAxis];
				}
				break;

			case EPCGExBoxFittingSeparation::EdgeDirection:
				{
					// Check if nodes are connected and use edge direction
					bool bConnected = false;
					for (const PCGExGraphs::FEdge& Edge : *Cluster->Edges)
					{
						if ((Cluster->GetEdgeStart(Edge)->Index == Node.Index && Cluster->GetEdgeEnd(Edge)->Index == OtherNodeIndex) ||
							(Cluster->GetEdgeStart(Edge)->Index == OtherNodeIndex && Cluster->GetEdgeEnd(Edge)->Index == Node.Index))
						{
							bConnected = true;
							break;
						}
					}

					if (bConnected)
					{
						SeparationDir = (OtherPos - CurrentPos).GetSafeNormal();
						SeparationMagnitude = FMath::Min3(OverlapSize.X, OverlapSize.Y, OverlapSize.Z);
					}
					else
					{
						// Fall back to minimum penetration for non-connected nodes
						const int32 MinAxis = (OverlapSize.X <= OverlapSize.Y && OverlapSize.X <= OverlapSize.Z) ? 0 : (OverlapSize.Y <= OverlapSize.Z) ? 1 : 2;
						SeparationDir = FVector::ZeroVector;
						SeparationDir[MinAxis] = (CurrentPos[MinAxis] < OtherPos[MinAxis]) ? -1.0 : 1.0;
						SeparationMagnitude = OverlapSize[MinAxis];
					}
				}
				break;

			case EPCGExBoxFittingSeparation::Centroid:
			default:
				{
					const FVector Delta = OtherPos - CurrentPos;
					const double Distance = Delta.Size();
					if (Distance <= KINDA_SMALL_NUMBER)
					{
						SeparationDir = FVector(1, 0, 0); // Arbitrary direction for coincident points
					}
					else
					{
						SeparationDir = Delta / Distance;
					}
					SeparationMagnitude = FMath::Min3(OverlapSize.X, OverlapSize.Y, OverlapSize.Z);
				}
				break;
			}

			// Apply separation force
			AddDelta(OtherNode->Index, Node.Index, RepulsionConstant * SeparationMagnitude * SeparationDir);
		}
	}

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> ExtentsBuffer;
};

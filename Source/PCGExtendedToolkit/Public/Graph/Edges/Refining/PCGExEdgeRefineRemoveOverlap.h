// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveOverlap.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Edge Overlap Pick"))
enum class EPCGExEdgeOverlapPick : uint8
{
	Shortest UMETA(DisplayName = "Shortest", ToolTip="Keep the shortest edge"),
	Longest UMETA(DisplayName = "Longest", ToolTip="Keep the longest edge"),
};

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Overlap"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRemoveOverlap : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual bool RequiresEdgeOctree() override { return true; }
	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics) override;
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override;
	//virtual void Process() override;

	/** Which edge to keep when doing comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgeOverlapPick Keep = EPCGExEdgeOverlapPick::Longest;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 0.001;
	double ToleranceSquared = 0.001;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = true;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MinAngle = 0;
	double MinDot = -1;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = true;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MaxAngle = 90;
	double MaxDot = 1;
};

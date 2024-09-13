// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveLineTrace.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove by Collision"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRemoveLineTrace : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	/** If the first linecast fails, tries the other way around. This is to ensure we don't fail against backfacing, but has high cost.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTwoWayCheck = true;

protected:
	FPCGExCollisionDetails InitializedCollisionSettings;
};

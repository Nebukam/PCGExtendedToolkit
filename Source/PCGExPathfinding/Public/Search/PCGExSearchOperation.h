// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"
#include "PCGExCoreMacros.h"

#include "UObject/Object.h"

#include "PCGExSearchOperation.generated.h"

namespace PCGExHeuristics
{
	class FLocalFeedbackHandler;
	class FHandler;
}

namespace PCGExPathfinding
{
	class FSearchAllocations;
	class FPathQuery;
	struct FExtraWeights;
}

class FPCGExHeuristicOperation;

namespace PCGExClusters
{
	class FCluster;
}

class FPCGExSearchOperation : public FPCGExOperation
{
public:
	bool bEarlyExit = true;
	PCGExClusters::FCluster* Cluster = nullptr;

	virtual void PrepareForCluster(PCGExClusters::FCluster* InCluster);
	virtual bool ResolveQuery(
		const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
		const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
		const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback = nullptr) const;

	virtual TSharedPtr<PCGExPathfinding::FSearchAllocations> NewAllocations() const;
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXELEMENTSPATHFINDING_API UPCGExSearchInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExSearchOperation> CreateOperation() const PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bEarlyExit = true;
};

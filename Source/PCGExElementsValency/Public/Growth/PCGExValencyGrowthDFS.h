// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyGrowthOperation.h"

#include "PCGExValencyGrowthDFS.generated.h"

/**
 * DFS (Depth-First Search) growth strategy.
 * Pops sockets from the back of the frontier (LIFO stack).
 * Produces tendril/corridor-like growth patterns.
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyGrowthDFS : public FPCGExValencyGrowthOperation
{
public:
	FPCGExValencyGrowthDFS() = default;
	virtual ~FPCGExValencyGrowthDFS() override = default;

protected:
	virtual int32 SelectNextSocket(TArray<FPCGExOpenSocket>& Frontier) override;
};

/**
 * Factory for DFS growth strategy.
 */
UCLASS(DisplayName = "DFS Growth", meta=(ToolTip = "Depth-first growth. Tendril/corridor-like expansion.", PCGExNodeLibraryDoc="valency/valency-generative/dfs-growth"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyGrowthDFSFactory : public UPCGExValencyGrowthFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExValencyGrowthOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(ValencyGrowthDFS)
		return NewOperation;
	}
};

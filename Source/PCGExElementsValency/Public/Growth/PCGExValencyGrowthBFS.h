// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyGrowthOperation.h"

#include "PCGExValencyGrowthBFS.generated.h"

/**
 * BFS (Breadth-First Search) growth strategy.
 * Pops sockets from the front of the frontier (FIFO queue).
 * Produces even radial expansion from seeds.
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyGrowthBFS : public FPCGExValencyGrowthOperation
{
public:
	FPCGExValencyGrowthBFS() = default;
	virtual ~FPCGExValencyGrowthBFS() override = default;

protected:
	virtual int32 SelectNextConnector(TArray<FPCGExOpenConnector>& Frontier) override;
};

/**
 * Factory for BFS growth strategy.
 */
UCLASS(DisplayName = "BFS Growth", meta=(ToolTip = "Breadth-first growth. Even radial expansion from seeds.", PCGExNodeLibraryDoc="valency/valency-generative/bfs-growth"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyGrowthBFSFactory : public UPCGExValencyGrowthFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExValencyGrowthOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(ValencyGrowthBFS)
		return NewOperation;
	}
};

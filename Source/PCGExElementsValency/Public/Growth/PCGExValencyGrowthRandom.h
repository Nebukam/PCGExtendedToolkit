// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyGrowthOperation.h"

#include "PCGExValencyGrowthRandom.generated.h"

/**
 * Random growth strategy.
 * Picks a random socket from the frontier.
 * Produces organic, unpredictable growth patterns.
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyGrowthRandom : public FPCGExValencyGrowthOperation
{
public:
	FPCGExValencyGrowthRandom() = default;
	virtual ~FPCGExValencyGrowthRandom() override = default;

protected:
	virtual int32 SelectNextSocket(TArray<FPCGExOpenSocket>& Frontier) override;
};

/**
 * Factory for random growth strategy.
 */
UCLASS(DisplayName = "Random Growth", meta=(ToolTip = "Random socket selection. Organic, unpredictable growth.", PCGExNodeLibraryDoc="valency/valency-generative/random-growth"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyGrowthRandomFactory : public UPCGExValencyGrowthFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExValencyGrowthOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(ValencyGrowthRandom)
		return NewOperation;
	}
};

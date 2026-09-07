// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExCollectionStagingContext.generated.h"

/**
 * Per-session scratch object for UPCGExCollectionStagingPipeline: created before OnPreRebuild, released
 * after OnPostRebuild, so what OnProcessEntry accumulates reaches OnPostRebuild and never leaks into the
 * next rebuild or the asset. Deliberately empty: subclass (Blueprint or C++) to add variables, name it
 * in the pipeline's ContextClass, reach it from hooks via GetContext.
 */
UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="Collection Staging Context"))
class PCGEXCOLLECTIONS_API UPCGExCollectionStagingContext : public UObject
{
	GENERATED_BODY()
};

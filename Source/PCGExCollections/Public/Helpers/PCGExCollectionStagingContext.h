// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExCollectionStagingContext.generated.h"

/**
 * Per-session scratch object for UPCGExCollectionStagingPipeline. A fresh instance is created
 * before OnPreRebuild and released after OnPostRebuild, so whatever OnProcessEntry accumulates on it
 * is visible in OnPostRebuild and never leaks into the next rebuild or into the collection asset.
 *
 * Deliberately empty: subclass in Blueprint (or C++) to add the variables a pipeline needs, and
 * name the subclass in the pipeline's ContextClass. Reach it from hooks via GetContext.
 */
UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="Collection Staging Context"))
class PCGEXCOLLECTIONS_API UPCGExCollectionStagingContext : public UObject
{
	GENERATED_BODY()
};

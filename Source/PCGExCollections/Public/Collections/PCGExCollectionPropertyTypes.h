// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPropertyCompiled.h"

#include "PCGExCollectionPropertyTypes.generated.h"

class UPCGExAssetCollection;

/**
 * Compiled property referencing a PCGExAssetCollection.
 * Used for mesh/actor swapping based on pattern matches.
 * Does not support direct attribute output.
 */
USTRUCT(BlueprintType)
struct PCGEXCOLLECTIONS_API FPCGExPropertyCompiled_AssetCollection : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	virtual FName GetTypeName() const override { return FName("AssetCollection"); }
};

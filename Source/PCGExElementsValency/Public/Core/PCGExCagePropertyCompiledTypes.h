// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCagePropertyCompiled.h"

#include "PCGExCagePropertyCompiledTypes.generated.h"

class UPCGExAssetCollection;

/**
 * Compiled property referencing a PCGExAssetCollection.
 * Used for mesh/actor swapping based on pattern matches.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled_AssetCollection : public FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;
};

/**
 * Compiled property containing generic key-value metadata.
 * For arbitrary user data that doesn't warrant a dedicated property type.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExCagePropertyCompiled_Metadata : public FPCGExCagePropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	TMap<FName, FString> Metadata;
};

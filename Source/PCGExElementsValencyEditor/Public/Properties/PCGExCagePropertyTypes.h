// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCagePropertyBase.h"

#include "PCGExCagePropertyTypes.generated.h"

class UPCGExAssetCollection;

/**
 * Property component referencing a PCGExAssetCollection.
 * Used for mesh/actor swapping based on pattern matches.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Asset Collection"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_AssetCollection : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	/** The asset collection to reference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	//~ Begin UPCGExCagePropertyBase Interface
	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
	//~ End UPCGExCagePropertyBase Interface
};

/**
 * Property component containing generic key-value metadata.
 * For arbitrary user data that doesn't warrant a dedicated property type.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Metadata"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Metadata : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	/** Key-value metadata pairs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TMap<FName, FString> Metadata;

	//~ Begin UPCGExCagePropertyBase Interface
	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
	//~ End UPCGExCagePropertyBase Interface
};

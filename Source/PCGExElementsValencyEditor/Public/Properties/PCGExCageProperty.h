// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"

#include "PCGExCageProperty.generated.h"

class UPCGExAssetCollection;

/**
 * Base class for cage property components.
 * Add these to cage actors to attach custom data that compiles into bonding rules.
 * Properties are accessible at solver, matcher, and replacement stages.
 */
UCLASS(Abstract, ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCagePropertyBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UPCGExCagePropertyBase();

	/**
	 * User-defined name for disambiguation when multiple properties exist.
	 * Must be unique across the entire ruleset for properties of different types.
	 * Leave empty if this cage only has one property of this type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FName PropertyName;

	/**
	 * Get the effective property name.
	 * Returns PropertyName if set, otherwise defaults to the component's name.
	 */
	FName GetEffectivePropertyName() const;

	/**
	 * Compile this property into a runtime-ready struct.
	 * @param OutCompiled - FInstancedStruct to populate with compiled data
	 * @return True if compilation succeeded
	 */
	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const PURE_VIRTUAL(UPCGExCagePropertyBase::CompileProperty, return false;);

	/**
	 * Get the UScriptStruct type this property compiles to.
	 * Used for type validation during ruleset compilation.
	 */
	virtual UScriptStruct* GetCompiledStructType() const PURE_VIRTUAL(UPCGExCagePropertyBase::GetCompiledStructType, return nullptr;);
};

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
 * Property component containing additional tags.
 * Supplements actor tags for more granular filtering.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Tags"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Tags : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	/** Additional tags for this cage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TArray<FName> Tags;

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

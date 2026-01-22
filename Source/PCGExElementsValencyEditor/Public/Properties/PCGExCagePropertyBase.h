// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"

#include "PCGExCagePropertyBase.generated.h"

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
	 * Leave empty to use the component's name as default.
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

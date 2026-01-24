// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGExPropertyCompiled.h"

#include "PCGExPropertyCollectionComponent.generated.h"

/**
 * Actor component for attaching property collections to any actor.
 * Runtime-compatible - can be used on any actor, not just editor cages.
 *
 * Valency scans for these on cages/patterns during compilation.
 * Other systems can scan for them on spawned actors at runtime.
 */
UCLASS(ClassGroup = "PCGEx", meta = (BlueprintSpawnableComponent, DisplayName = "PCGEx Property Collection"))
class PCGEXPROPERTIES_API UPCGExPropertyCollectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPCGExPropertyCollectionComponent();

	/**
	 * Property collection with schema definitions and default values.
	 * These compile into runtime property data during cage/pattern builds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FPCGExPropertySchemaCollection Properties;

	/**
	 * Get the property collection.
	 * @return Reference to the property schema collection
	 */
	const FPCGExPropertySchemaCollection& GetProperties() const { return Properties; }

	/**
	 * Get the property collection (mutable).
	 * @return Reference to the property schema collection
	 */
	FPCGExPropertySchemaCollection& GetPropertiesMutable() { return Properties; }
};

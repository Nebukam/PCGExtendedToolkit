// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"

#include "PCGExValencyCageWildcard.generated.h"

class USphereComponent;

/**
 * A wildcard cage representing "any neighbor required here".
 * Wildcard cages indicate that a connection MUST exist at this orbital,
 * but any module type is acceptable.
 *
 * This is the inverse of Null Cage:
 * - Null Cage: "no neighbor allowed" (boundary)
 * - Wildcard Cage: "any neighbor required" (must have connection)
 *
 * Visualized as a small magenta sphere for easy selection.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Valency Cage (Wildcard)"))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCageWildcard : public APCGExValencyCageBase
{
	GENERATED_BODY()

public:
	APCGExValencyCageWildcard();

	//~ Begin AActor Interface
	virtual void PostEditMove(bool bFinished) override;
	//~ End AActor Interface

	//~ Begin APCGExValencyCageBase Interface
	virtual FString GetCageDisplayName() const override;
	virtual bool IsWildcardCage() const override { return true; }
	virtual void SetDebugComponentsVisible(bool bVisible) override;
	//~ End APCGExValencyCageBase Interface

public:
	/**
	 * Optional description of this wildcard constraint.
	 * E.g., "Must connect to something", "Any neighbor OK", etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildcard Cage")
	FString Description;

protected:
	/** Sphere component for visualization and selection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DebugSphereComponent;
};

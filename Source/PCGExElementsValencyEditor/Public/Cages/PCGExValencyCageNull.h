// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"

#include "PCGExValencyCageNull.generated.h"

class UBillboardComponent;

/**
 * A null cage representing a boundary or intentional dead-end.
 * Null cages indicate "no neighbor here" for leaf nodes and edges.
 *
 * Visualized as a billboard sprite rather than a full cage.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Valency Cage (Null)"))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCageNull : public APCGExValencyCageBase
{
	GENERATED_BODY()

public:
	APCGExValencyCageNull();

	//~ Begin APCGExValencyCageBase Interface
	virtual FString GetCageDisplayName() const override;
	virtual bool IsNullCage() const override { return true; }
	//~ End APCGExValencyCageBase Interface

public:
	/**
	 * Optional description of why this is a null cage.
	 * E.g., "Edge of map", "Wall boundary", etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Null Cage")
	FString Description;

protected:
	/** Billboard component for visualization */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBillboardComponent> BillboardComponent;
};

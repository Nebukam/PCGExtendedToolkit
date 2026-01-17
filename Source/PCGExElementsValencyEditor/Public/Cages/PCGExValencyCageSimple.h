// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/ShapeComponent.h"
#include "PCGExValencyCage.h"

#include "PCGExValencyCageSimple.generated.h"

/**
 * Shape type for simple cage containment detection.
 */
UENUM(BlueprintType)
enum class EPCGExValencyCageShape : uint8
{
	Box      UMETA(DisplayName = "Box"),
	Sphere   UMETA(DisplayName = "Sphere"),
	Cylinder UMETA(DisplayName = "Cylinder")
};

/**
 * A Valency cage with built-in shape-based containment detection.
 * Supports box, sphere, and cylinder shapes for asset detection.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Valency Cage (Simple)"))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCageSimple : public APCGExValencyCage
{
	GENERATED_BODY()

public:
	APCGExValencyCageSimple();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End AActor Interface

	//~ Begin APCGExValencyCage Interface
	virtual bool IsActorInside_Implementation(AActor* Actor) const override;
	virtual bool ContainsPoint_Implementation(const FVector& WorldLocation) const override;
	//~ End APCGExValencyCage Interface

	/** Get the bounding box for this cage (used for visualization) */
	FBox GetBoundingBox() const;

public:
	/** Shape used for containment detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection")
	EPCGExValencyCageShape DetectionShape = EPCGExValencyCageShape::Box;

	/** Half-extents for box shape (X, Y, Z from center) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (EditCondition = "DetectionShape == EPCGExValencyCageShape::Box", EditConditionHides, ClampMin = "0.0"))
	FVector BoxExtent = FVector(50.0f);

	/** Radius for sphere shape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (EditCondition = "DetectionShape == EPCGExValencyCageShape::Sphere", EditConditionHides, ClampMin = "0.0"))
	float SphereRadius = 50.0f;

	/** Radius for cylinder shape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (EditCondition = "DetectionShape == EPCGExValencyCageShape::Cylinder", EditConditionHides, ClampMin = "0.0"))
	float CylinderRadius = 50.0f;

	/** Half-height for cylinder shape (extends up and down from center) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Detection", meta = (EditCondition = "DetectionShape == EPCGExValencyCageShape::Cylinder", EditConditionHides, ClampMin = "0.0"))
	float CylinderHalfHeight = 50.0f;

protected:
	/** Recreate the debug shape component for the current DetectionShape */
	void RecreateDebugShape();

	/** Update the current debug shape's dimensions */
	void UpdateDebugShapeDimensions();

	/** Current debug visualization component (type depends on DetectionShape) */
	UPROPERTY(VisibleAnywhere, Category = "Cage|Debug")
	TObjectPtr<UShapeComponent> DebugShapeComponent;

	/** Cached shape type to detect when recreation is needed */
	EPCGExValencyCageShape CachedShapeType = EPCGExValencyCageShape::Box;
};

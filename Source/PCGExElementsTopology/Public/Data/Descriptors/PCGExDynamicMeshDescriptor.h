// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "SceneTypes.h"
#include "Data/Descriptors/PCGExComponentDescriptors.h"
#include "Engine/EngineTypes.h"

#include "PCGExDynamicMeshDescriptor.generated.h"

USTRUCT(BlueprintType, DisplayName="[PCGEx] Dynamic Mesh Component Descriptor")
struct PCGEXELEMENTSTOPOLOGY_API FPCGExDynamicMeshDescriptor : public FPCGExMeshComponentDescriptor
{
	GENERATED_BODY()

	explicit FPCGExDynamicMeshDescriptor(ENoInit)
		: FPCGExMeshComponentDescriptor(NoInit)
	{
	}

	FPCGExDynamicMeshDescriptor();

#pragma region Properties

	/**
	 *	Controls whether the physics cooking should be done off the game thread.
	 *  This should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dynamic Mesh Component|Collision", meta=(PCG_Overridable))
	bool bUseAsyncCooking = false;

	/** 
	 * If true, current mesh will be used as Complex Collision source mesh. 
	 * This is independent of the CollisionType setting, ie, even if Complex collision is enabled, if this is false, then the Complex Collision mesh will be empty
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Collision", meta=(PCG_Overridable))
	bool bEnableComplexCollision = false;

	/** If true, updates to the mesh will not result in immediate collision regeneration. Useful when the mesh will be modified multiple times before collision is needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Collision", meta=(PCG_Overridable))
	bool bDeferCollisionUpdates = false;

	/**
	 * If true, render the Wireframe on top of the Shaded Mesh
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Wireframe Overlay"))
	bool bExplicitShowWireframe = false;

	/**
	 * Constant Color used when Override Color Mode is set to Constant
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Wireframe Color"))
	FLinearColor WireframeColor = FLinearColor(0, 0.5f, 1.f);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Color Override"))
	EDynamicMeshComponentColorOverrideMode ColorMode = EDynamicMeshComponentColorOverrideMode::None;

	/**
	 * Constant Color used when Override Color Mode is set to Constant
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Constant Color", EditCondition = "ColorMode == EDynamicMeshComponentColorOverrideMode::Constant"))
	FColor ConstantColor = FColor::White;

	/**
	 * Color Space Transform that will be applied to the colors stored in the DynamicMesh Attribute Color Overlay when
	 * constructing render buffers. 
	 * Default is "No Transform", ie color R/G/B/A will be independently converted from 32-bit float to 8-bit by direct mapping.
	 * LinearToSRGB mode will apply SRGB conversion, ie assumes colors in the Mesh are in Linear space. This will produce the same behavior as UStaticMesh.
	 * SRGBToLinear mode will invert SRGB conversion, ie assumes colors in the Mesh are in SRGB space. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Vertex Color Space"))
	EDynamicMeshVertexColorTransformMode ColorSpaceMode = EDynamicMeshVertexColorTransformMode::NoTransform;

	/**
	 * Enable use of per-triangle facet normals in place of mesh normals
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Flat Shading"))
	bool bEnableFlatShading = false;

	/** 
	 * This flag controls whether Editor View Mode Overrides are enabled for this mesh. For example, this controls hidden-line removal on the wireframe 
	 * in Wireframe View Mode, and whether the normal map will be disabled in Lighting-Only View Mode, as well as various other things.
	 * Use SetViewModeOverridesEnabled() to control this setting in Blueprints/C++.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "View Mode Overrides"))
	bool bEnableViewModeOverrides = true;

	/**
	 * Enable/disable Raytracing support on this Mesh, if Raytracing is currently enabled in the Project Settings.
	 * Use SetEnableRaytracing() to configure this flag in Blueprints/C++.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering")
	bool bEnableRaytracing = true;

#pragma endregion

	virtual void InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance = false) override;
	virtual void InitComponent(UPrimitiveComponent* InComponent) const override;
};

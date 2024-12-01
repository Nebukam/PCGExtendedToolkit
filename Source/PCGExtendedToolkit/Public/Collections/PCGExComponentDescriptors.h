// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "SceneTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "Engine/EngineTypes.h"

#include "PCGExComponentDescriptors.generated.h"


USTRUCT(BlueprintType, DisplayName="[PCGEx] Primitive Component Descriptor")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPrimitiveComponentDescriptor
{
	GENERATED_BODY()

	FPCGExPrimitiveComponentDescriptor();

	explicit FPCGExPrimitiveComponentDescriptor(ENoInit)
	{
	}

	virtual ~FPCGExPrimitiveComponentDescriptor() = default;

#pragma region Properties

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	bool bVisible = true;

	/**
	 * The minimum distance at which the primitive should be rendered, 
	 * measured in world space units from the center of the primitive's bounding sphere to the camera position.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	float MinDrawDistance = 0.0;

	/**  Max draw distance exposed to LDs. The real max draw distance is the min (disregarding 0) of this and volumes affecting this object. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD, meta=(DisplayName="Desired Max Draw Distance"))
	float LDMaxDrawDistance = 0.0;

	/** Quality of indirect lighting for Movable primitives.  This has a large effect on Indirect Lighting Cache update time. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	TEnumAsByte<EIndirectLightingCacheQuality> IndirectLightingCacheQuality;

	/** Controls the type of lightmap used for this component. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	ELightmapType LightmapType = ELightmapType::Default;

	/** Determines how the geometry of a component will be incorporated in proxy (simplified) HLODs. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=HLOD, meta=(DisplayName="HLOD Batching Policy", DisplayAfter="bEnableAutoLODGeneration", EditConditionHides, EditCondition="bEnableAutoLODGeneration"))
	EHLODBatchingPolicy HLODBatchingPolicy = EHLODBatchingPolicy::None;

	/** Whether to include this component in HLODs or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HLOD, meta=(DisplayName="Include Component in HLOD"))
	uint8 bEnableAutoLODGeneration : 1;

	/** When enabled this object will not be culled by distance. This is ignored if a child of a HLOD. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	uint8 bNeverDistanceCull : 1;

	/** Physics scene information for this component, holds a single rigid body with multiple shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision, meta=(ShowOnlyInnerProperties, SkipUCSModifiedProperties))
	FBodyInstance BodyInstance;

	/** 
	 * Indicates if we'd like to create physics state all the time (for collision and simulation). 
	 * If you set this to false, it still will create physics state if collision or simulation activated. 
	 * This can help performance if you'd like to avoid overhead of creating physics state when triggers 
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Collision)
	uint8 bAlwaysCreatePhysicsState : 1;

	/**
	 * If true, this component will generate individual overlaps for each overlapping physics body if it is a multi-body component. When false, this component will
	 * generate only one overlap, regardless of how many physics bodies it has and how many of them are overlapping another component/body. This flag has no
	 * influence on single body components.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bMultiBodyOverlap : 1;

	/**
	 * If true, component sweeps with this component should trace against complex collision during movement (for example, each triangle of a mesh).
	 * If false, collision will be resolved against simple collision bounds instead.
	 * @see MoveComponent()
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bTraceComplexOnMove : 1;

	/**
	 * If true, component sweeps will return the material in their hit result.
	 * @see MoveComponent(), FHitResult
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bReturnMaterialOnMove : 1;

	/** Whether to accept cull distance volumes to modify cached cull distance. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	uint8 bAllowCullDistanceVolume : 1;

	/** If true, this component will be visible in reflection captures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInReflectionCaptures : 1;

	/** If true, this component will be visible in real-time sky light reflection captures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInRealTimeSkyCaptures : 1;

	/** If true, this component will be visible in ray tracing effects. Turning this off will remove it from ray traced reflections, shadows, etc. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInRayTracing : 1;

	/** If true, this component will be rendered in the main pass (z prepass, basepass, transparency) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bRenderInMainPass : 1;

	/** If true, this component will be rendered in the depth pass even if it's not rendered in the main pass */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (EditCondition = "!bRenderInMainPass"))
	uint8 bRenderInDepthPass : 1;

	/** Whether the primitive receives decals. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bReceivesDecals : 1;

	/** If this is True, this primitive will render black with an alpha of 0, but all secondary effects (shadows, reflections, indirect lighting) remain. This feature required the project setting "Enable alpha channel support in post processing". */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, Interp)
	uint8 bHoldout : 1;

	/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bOwnerNoSee : 1;

	/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bOnlyOwnerSee : 1;

	/** Treat this primitive as part of the background for occlusion purposes. This can be used as an optimization to reduce the cost of rendering skyboxes, large ground planes that are part of the vista, etc. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bTreatAsBackgroundForOcclusion : 1;

	/** 
	 * Whether to render the primitive in the depth only pass.  
	 * This should generally be true for all objects, and let the renderer make decisions about whether to render objects in the depth only pass.
	 * @todo - if any rendering features rely on a complete depth only pass, this variable needs to go away.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bUseAsOccluder : 1;

	/** If true, forces mips for textures used by this component to be resident when this component's level is loaded. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=TextureStreaming)
	uint8 bForceMipStreaming : 1;

	// Lighting flags

	/** Controls whether the primitive component should cast a shadow or not. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, Interp)
	uint8 CastShadow : 1;

	/** Whether the primitive will be used as an emissive light source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay)
	uint8 bEmissiveLightSource : 1;

	/** Controls whether the primitive should influence indirect lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, Interp)
	uint8 bAffectDynamicIndirectLighting : 1;

	/** Controls whether the primitive should affect indirect lighting when hidden. This flag is only used if bAffectDynamicIndirectLighting is true. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="bAffectDynamicIndirectLighting", DisplayName = "Affect Indirect Lighting While Hidden"), Interp)
	uint8 bAffectIndirectLightingWhileHidden : 1;

	/** Controls whether the primitive should affect dynamic distance field lighting methods.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay)
	uint8 bAffectDistanceFieldLighting : 1;

	/** Controls whether the primitive should cast shadows in the case of non precomputed shadowing.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Dynamic Shadow"))
	uint8 bCastDynamicShadow : 1;

	/** Whether the object should cast a static shadow from shadow casting lights.  This flag is only used if CastShadow is true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Static Shadow"))
	uint8 bCastStaticShadow : 1;

	/** Control shadow invalidation behavior, in particular with respect to Virtual Shadow Maps and material effects like World Position Offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow"))
	EShadowCacheInvalidationBehavior ShadowCacheInvalidationBehavior;

	/** 
	 * Whether the object should cast a volumetric translucent shadow.
	 * Volumetric translucent shadows are useful for primitives with smoothly changing opacity like particles representing a volume, 
	 * But have artifacts when used on highly opaque surfaces.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Volumetric Translucent Shadow"))
	uint8 bCastVolumetricTranslucentShadow : 1;

	/**
	 * Whether the object should cast contact shadows.
	 * This flag is only used if CastShadow is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Contact Shadow"))
	uint8 bCastContactShadow : 1;

	/** 
	 * When enabled, the component will only cast a shadow on itself and not other components in the world.  
	 * This is especially useful for first person weapons, and forces bCastInsetShadow to be enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow"))
	uint8 bSelfShadowOnly : 1;

	/** 
	 * When enabled, the component will be rendering into the far shadow cascades (only for directional lights).
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Far Shadow"))
	uint8 bCastFarShadow : 1;

	/** 
	 * Whether this component should create a per-object shadow that gives higher effective shadow resolution. 
	 * Useful for cinematic character shadowing. Assumed to be enabled if bSelfShadowOnly is enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Dynamic Inset Shadow"))
	uint8 bCastInsetShadow : 1;

	/** 
	 * Whether this component should cast shadows from lights that have bCastShadowsFromCinematicObjectsOnly enabled.
	 * This is useful for characters in a cinematic with special cinematic lights, where the cost of shadowmap rendering of the environment is undesired.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow"))
	uint8 bCastCinematicShadow : 1;

	/** 
	 *	If true, the primitive will cast shadows even if bHidden is true.
	 *	Controls whether the primitive should cast shadows when hidden.
	 *	This flag is only used if CastShadow is true.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Hidden Shadow"), Interp)
	uint8 bCastHiddenShadow : 1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Shadow Two Sided"))
	uint8 bCastShadowAsTwoSided : 1;

	/** 
	 * Whether to light this component and any attachments as a group.  This only has effect on the root component of an attachment tree.
	 * When enabled, attached component shadowing settings like bCastInsetShadow, bCastVolumetricTranslucentShadow, etc, will be ignored.
	 * This is useful for improving performance when multiple movable components are attached together.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bLightAttachmentsAsGroup : 1;

	/** 
	 * If set, then it overrides any bLightAttachmentsAsGroup set in a parent.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bExcludeFromLightAttachmentGroup : 1;

	/**
	* Mobile only:
	* If disabled this component will not receive CSM shadows. (Components that do not receive CSM may have reduced shading cost)
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Mobile, meta = (DisplayName = "Receive CSM Shadows"))
	uint8 bReceiveMobileCSMShadows : 1;

	/** 
	 * Whether the whole component should be shadowed as one from stationary lights, which makes shadow receiving much cheaper.
	 * When enabled shadowing data comes from the volume lighting samples precomputed by Lightmass, which are very sparse.
	 * This is currently only used on stationary directional lights.  
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bSingleSampleShadowFromStationaryLights : 1;

	// Physics

	/** Will ignore radial impulses applied to this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bIgnoreRadialImpulse : 1;

	/** Will ignore radial forces applied to this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bIgnoreRadialForce : 1;

	/** True for damage to this component to apply physics impulse, false to opt out of these impulses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bApplyImpulseOnDamage : 1;

	/** True if physics should be replicated to autonomous proxies. This should be true for
		server-authoritative simulations, and false for client authoritative simulations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bReplicatePhysicsToAutonomousProxy : 1;

	// Navigation

	/** If set, navmesh will not be generated under the surface of the geometry */
	UPROPERTY(EditAnywhere, Category = Navigation)
	uint8 bFillCollisionUnderneathForNavmesh : 1;

	// General flags.

	/** If true, this component will be rendered in the CustomDepth pass (usually used for outlines) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(DisplayName = "Render CustomDepth Pass"))
	uint8 bRenderCustomDepth : 1;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (DisplayName = "Visible In Scene Capture Only", ToolTip = "When true, will only be visible in Scene Capture"))
	uint8 bVisibleInSceneCaptureOnly : 1;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (DisplayName = "Hidden In Scene Capture", ToolTip = "When true, will not be captured by Scene Capture"))
	uint8 bHiddenInSceneCapture : 1;

	/**
	 * Determine whether a Character can step up onto this component.
	 * This controls whether they can try to step up on it when they bump in to it, not whether they can walk on it after landing on it.
	 * @see FWalkableSlopeOverride
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
	TEnumAsByte<ECanBeCharacterBase> CanCharacterStepUpOn;

	/** 
	 * Channels that this component should be in.  Lights with matching channels will affect the component.  
	 * These channels only apply to opaque materials, direct lighting, and dynamic lighting and shadowing.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	FLightingChannels LightingChannels;

	/**
	 * Defines run-time groups of components. For example allows to assemble multiple parts of a building at runtime.
	 * -1 means that component doesn't belong to any group.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RayTracing)
	int32 RayTracingGroupId;

	/** Optionally write this 0-255 value to the stencil buffer in CustomDepth pass (Requires project setting or r.CustomDepth == 3) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(UIMin = "0", UIMax = "255", editcondition = "bRenderCustomDepth", DisplayName = "CustomDepth Stencil Value"))
	int32 CustomDepthStencilValue;

	/**
	 * Translucent objects with a lower sort priority draw behind objects with a higher priority.
	 * Translucent objects with the same priority are rendered from back-to-front based on their bounds origin.
	 * This setting is also used to sort objects being drawn into a runtime virtual texture.
	 *
	 * Ignored if the object is not translucent.  The default priority is zero.
	 * Warning: This should never be set to a non-default value unless you know what you are doing, as it will prevent the renderer from sorting correctly.  
	 * It is especially problematic on dynamic gameplay effects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	int32 TranslucencySortPriority;

	/**
	 * Modified sort distance offset for translucent objects in world units.
	 * A positive number will move the sort distance further and a negative number will move the distance closer.
	 *
	 * Ignored if the object is not translucent.
	 * Warning: Adjusting this value will prevent the renderer from correctly sorting based on distance.  Only modify this value if you are certain it will not cause visual artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Rendering)
	float TranslucencySortDistanceOffset = 0.0f;

	/** 
	 * Array of runtime virtual textures into which we draw the mesh for this actor. 
	 * The material also needs to be set up to output to a virtual texture. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VirtualTexture, meta = (DisplayName = "Draw in Virtual Textures"))
	TArray<TSoftObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures;

	/** Bias to the LOD selected for rendering to runtime virtual textures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (DisplayName = "Virtual Texture LOD Bias", UIMin = "-7", UIMax = "8"))
	int8 VirtualTextureLodBias = 0;

	/**
	 * Number of lower mips in the runtime virtual texture to skip for rendering this primitive.
	 * Larger values reduce the effective draw distance in the runtime virtual texture.
	 * This culling method doesn't take into account primitive size or virtual texture size.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (DisplayName = "Virtual Texture Skip Mips", UIMin = "0", UIMax = "7"))
	int8 VirtualTextureCullMips = 0;

	/**
	 * Set the minimum pixel coverage before culling from the runtime virtual texture.
	 * Larger values reduce the effective draw distance in the runtime virtual texture.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (UIMin = "0", UIMax = "7"))
	int8 VirtualTextureMinCoverage = 0;

	/** Controls if this component draws in the main pass as well as in the virtual texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VirtualTexture, meta = (DisplayName = "Draw in Main Pass"))
	ERuntimeVirtualTextureMainPassType VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;

	/** 
	 * Scales the bounds of the object.
	 * This is useful when using World Position Offset to animate the vertices of the object outside of its bounds. 
	 * Warning: Increasing the bounds of an object will reduce performance and shadow quality!
	 * Currently only used by StaticMeshComponent and SkeletalMeshComponent.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Rendering, meta=(UIMin = "1", UIMax = "10.0"))
	float BoundsScale;

	// Internal physics engine data.

	/**
	 * Defines how quickly it should be culled. For example buildings should have a low priority, but small dressing should have a high priority.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RayTracing)
	ERayTracingGroupCullingPriority RayTracingGroupCullingPriority;

	/** Mask used for stencil buffer writes. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Rendering", meta = (editcondition = "bRenderCustomDepth"))
	ERendererStencilMask CustomDepthStencilWriteMask;

#pragma endregion

	virtual void InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance = false);
	virtual void InitComponent(UPrimitiveComponent* InComponent) const;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Component Descriptor")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshComponentDescriptor : public FPCGExPrimitiveComponentDescriptor
{
	GENERATED_BODY()

	FPCGExMeshComponentDescriptor();

	explicit FPCGExMeshComponentDescriptor(ENoInit)
	{
	}

#pragma region Properties

	/** Per-Component material overrides.  These must NOT be set directly or a race condition can occur between GC and the rendering thread. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Rendering, Meta=(ToolTip="Material overrides."))
	TArray<TSoftObjectPtr<UMaterialInterface>> OverrideMaterials;

	/** Translucent material to blend on top of this mesh. Mesh will be rendered twice - once with a base material and once with overlay material */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	TSoftObjectPtr<UMaterialInterface> OverlayMaterial;

	/** The max draw distance for overlay material. A distance of 0 indicates that overlay will be culled using primitive max distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	float OverlayMaterialMaxDrawDistance = 0.0;

#pragma endregion

	virtual void InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance = false) override;
	virtual void InitComponent(UPrimitiveComponent* InComponent) const override;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Static Mesh Component Descriptor")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExStaticMeshComponentDescriptor : public FPCGExMeshComponentDescriptor
{
	GENERATED_BODY()

	FPCGExStaticMeshComponentDescriptor();

	explicit FPCGExStaticMeshComponentDescriptor(ENoInit)
	{
	}

#pragma region Properties

	/** If 0, auto-select LOD level. if >0, force to (ForcedLodModel-1). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	int32 ForcedLodModel;

	/** 
	 * Specifies the smallest LOD that will be used for this component.  
	 * This is ignored if ForcedLodModel is enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD, meta=(editcondition = "bOverrideMinLOD"))
	int32 MinLOD;

	/** Wireframe color to use if bOverrideWireframeColor is true */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(editcondition = "bOverrideWireframeColor"))
	FColor WireframeColorOverride;

	/** Forces this component to always use Nanite for masked materials, even if FNaniteSettings::bAllowMaskedMaterials=false */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Rendering)
	uint8 bForceNaniteForMasked : 1;

	/** Forces this component to use fallback mesh for rendering if Nanite is enabled on the mesh. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Rendering)
	uint8 bDisallowNanite : 1;

	/** 
	 * Whether to evaluate World Position Offset. 
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Rendering)
	uint8 bEvaluateWorldPositionOffset : 1;

	/** 
	 * Whether world position offset turns on velocity writes.
	 * If the WPO isn't static then setting false may give incorrect motion vectors.
	 * But if we know that the WPO is static then setting false may save performance.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Rendering)
	uint8 bWorldPositionOffsetWritesVelocity : 1;

	/** 
	 * Whether to evaluate World Position Offset for ray tracing. 
	 * This is only used when running with r.RayTracing.Geometry.StaticMeshes.WPO=1 
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = RayTracing)
	uint8 bEvaluateWorldPositionOffsetInRayTracing : 1;

	/**
	 * Distance at which to disable World Position Offset for an entire instance (0 = Never disable WPO).
	 **/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	int32 WorldPositionOffsetDisableDistance = 0;

	/** If true, WireframeColorOverride will be used. If false, color is determined based on mobility and physics simulation settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Rendering, meta=(InlineEditConditionToggle))
	uint8 bOverrideWireframeColor : 1;

	/** Whether to override the MinLOD setting of the static mesh asset with the MinLOD of this component. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	uint8 bOverrideMinLOD : 1;

	/** If true, mesh painting is disallowed on this instance. Set if vertex colors are overridden in a construction script. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Rendering)
	uint8 bDisallowMeshPaintPerInstance : 1;

	/**
	 *	Ignore this instance of this static mesh when calculating streaming information.
	 *	This can be useful when doing things like applying character textures to static geometry,
	 *	to avoid them using distance-based streaming.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=TextureStreaming)
	uint8 bIgnoreInstanceForTextureStreaming : 1;

	/** Whether to override the lightmap resolution defined in the static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, meta=(InlineEditConditionToggle))
	uint8 bOverrideLightMapRes : 1;

	/** 
	 * Whether to use the mesh distance field representation (when present) for shadowing indirect lighting (from lightmaps or skylight) on Movable components.
	 * This works like capsule shadows on skeletal meshes, except using the mesh distance field so no physics asset is required.
	 * The StaticMesh must have 'Generate Mesh Distance Field' enabled, or the project must have 'Generate Mesh Distance Fields' enabled for this feature to work.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(DisplayName = "Distance Field Indirect Shadow"))
	uint8 bCastDistanceFieldIndirectShadow : 1;

	/** Whether to override the DistanceFieldSelfShadowBias setting of the static mesh asset with the DistanceFieldSelfShadowBias of this component. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bOverrideDistanceFieldSelfShadowBias : 1;

	/** Use the collision profile specified in the StaticMesh asset.*/
	UPROPERTY(EditAnywhere, Category = Collision)
	uint8 bUseDefaultCollision : 1;

	UPROPERTY(EditAnywhere, Category = Collision)
	uint8 bGenerateOverlapEvents : 1;

	/** Enable dynamic sort mesh's triangles to remove ordering issue when rendered with a translucent material */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Lighting, meta = (UIMin = "0", UIMax = "1", DisplayName = "Sort Triangles"))
	uint8 bSortTriangles : 1;

	/**
	 * Controls whether the static mesh component's backface culling should be reversed
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bReverseCulling : 1;

	/** Light map resolution to use on this component, used if bOverrideLightMapRes is true and there is a valid StaticMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, meta=(ClampMax = 4096, editcondition="bOverrideLightMapRes"))
	int32 OverriddenLightMapRes;

	/** 
	 * Controls how dark the dynamic indirect shadow can be.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(UIMin = "0", UIMax = "1", DisplayName = "Distance Field Indirect Shadow Min Visibility"))
	float DistanceFieldIndirectShadowMinVisibility;

	/** Useful for reducing self shadowing from distance field methods when using world position offset to animate the mesh's vertices. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	float DistanceFieldSelfShadowBias;

	/**
	 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
	 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
	 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
	 * Value can be < 0 (from legcay content, or code changes)
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=TextureStreaming, meta=(ClampMin = 0, ToolTip="Allows adjusting the desired resolution of streaming textures that uses UV 0.  1.0 is the default, whereas a higher value increases the streamed-in resolution."))
	float StreamingDistanceMultiplier;

	/** The Lightmass settings for this object. */
	UPROPERTY(EditAnywhere, Category=Lighting)
	FLightmassPrimitiveSettings LightmassSettings;

#pragma endregion

	virtual void InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance = false) override;
	virtual void InitComponent(UPrimitiveComponent* InComponent) const override;
};


UENUM(BlueprintType)
enum class EPCGExDynamicMeshComponentDistanceFieldMode : uint8
{
	NoDistanceField       = 0 UMETA(DisplayName = "No Distance Field"),
	AsyncCPUDistanceField = 1 UMETA(DisplayName = "Async CPU Distance Field"),
};


USTRUCT(BlueprintType, DisplayName="[PCGEx] Dynamic Mesh Component Descriptor")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDynamicMeshDescriptor : public FPCGExMeshComponentDescriptor
{
	GENERATED_BODY()

	explicit FPCGExDynamicMeshDescriptor(ENoInit)
		: FPCGExMeshComponentDescriptor(NoInit)
	{
	}

	FPCGExDynamicMeshDescriptor();

#pragma region Properties

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Dynamic Mesh Component|Rendering", meta = (PCG_Overridable))
	EPCGExDynamicMeshComponentDistanceFieldMode DistanceFieldMode = EPCGExDynamicMeshComponentDistanceFieldMode::NoDistanceField;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Mesh Component|Rendering", meta = (DisplayName = "Constant Color", EditCondition = "ColorMode==EDynamicMeshComponentColorOverrideMode::Constant"))
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

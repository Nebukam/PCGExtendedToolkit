// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExComponentDescriptors.h"
#include "PCGExMacros.h"

FPCGExPrimitiveComponentDescriptor::FPCGExPrimitiveComponentDescriptor()
{
	// Make sure we have proper defaults
	InitFrom(UPrimitiveComponent::StaticClass()->GetDefaultObject<UPrimitiveComponent>());
}

void FPCGExPrimitiveComponentDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	const UPrimitiveComponent* SourceComponent = Component;

	MinDrawDistance = SourceComponent->MinDrawDistance;
	LDMaxDrawDistance = SourceComponent->LDMaxDrawDistance;
	CachedMaxDrawDistance = SourceComponent->CachedMaxDrawDistance;
	IndirectLightingCacheQuality = SourceComponent->IndirectLightingCacheQuality;
#if PCGEX_ENGINE_VERSION < 505
	LightmapType = SourceComponent->LightmapType;
#else
	LightmapType = SourceComponent->GetLightmapType();
#endif
	HLODBatchingPolicy = SourceComponent->HLODBatchingPolicy;
	bEnableAutoLODGeneration = SourceComponent->bEnableAutoLODGeneration;
	bNeverDistanceCull = SourceComponent->bNeverDistanceCull;
	if (bInitBodyInstance) { BodyInstance.CopyBodyInstancePropertiesFrom(SourceComponent->GetBodyInstance()); }
	bAlwaysCreatePhysicsState = SourceComponent->bAlwaysCreatePhysicsState;
	bMultiBodyOverlap = SourceComponent->bMultiBodyOverlap;
	bTraceComplexOnMove = SourceComponent->bTraceComplexOnMove;
	bReturnMaterialOnMove = SourceComponent->bReturnMaterialOnMove;
	bAllowCullDistanceVolume = SourceComponent->bAllowCullDistanceVolume;
	bVisibleInReflectionCaptures = SourceComponent->bVisibleInReflectionCaptures;
	bVisibleInRealTimeSkyCaptures = SourceComponent->bVisibleInRealTimeSkyCaptures;
	bVisibleInRayTracing = SourceComponent->bVisibleInRayTracing;
	bRenderInMainPass = SourceComponent->bRenderInMainPass;
	bRenderInDepthPass = SourceComponent->bRenderInDepthPass;
	bReceivesDecals = SourceComponent->bReceivesDecals;
	bHoldout = SourceComponent->bHoldout;
	bOwnerNoSee = SourceComponent->bOwnerNoSee;
	bOnlyOwnerSee = SourceComponent->bOnlyOwnerSee;
	bTreatAsBackgroundForOcclusion = SourceComponent->bTreatAsBackgroundForOcclusion;
	bUseAsOccluder = SourceComponent->bUseAsOccluder;
	bForceMipStreaming = SourceComponent->bForceMipStreaming;
	CastShadow = SourceComponent->CastShadow;
	bEmissiveLightSource = SourceComponent->bEmissiveLightSource;
	bAffectDynamicIndirectLighting = SourceComponent->bAffectDynamicIndirectLighting;
	bAffectIndirectLightingWhileHidden = SourceComponent->bAffectIndirectLightingWhileHidden;
	bAffectDistanceFieldLighting = SourceComponent->bAffectDistanceFieldLighting;
	bCastDynamicShadow = SourceComponent->bCastDynamicShadow;
	bCastStaticShadow = SourceComponent->bCastStaticShadow;
	ShadowCacheInvalidationBehavior = SourceComponent->ShadowCacheInvalidationBehavior;
	bCastVolumetricTranslucentShadow = SourceComponent->bCastVolumetricTranslucentShadow;
	bCastContactShadow = SourceComponent->bCastContactShadow;
	bSelfShadowOnly = SourceComponent->bSelfShadowOnly;
	bCastFarShadow = SourceComponent->bCastFarShadow;
	bCastInsetShadow = SourceComponent->bCastInsetShadow;
	bCastCinematicShadow = SourceComponent->bCastCinematicShadow;
	bCastHiddenShadow = SourceComponent->bCastHiddenShadow;
	bCastShadowAsTwoSided = SourceComponent->bCastShadowAsTwoSided;
	bLightAttachmentsAsGroup = SourceComponent->bLightAttachmentsAsGroup;
	bExcludeFromLightAttachmentGroup = SourceComponent->bExcludeFromLightAttachmentGroup;
	bReceiveMobileCSMShadows = SourceComponent->bReceiveMobileCSMShadows;
	bSingleSampleShadowFromStationaryLights = SourceComponent->bSingleSampleShadowFromStationaryLights;
	bIgnoreRadialImpulse = SourceComponent->bIgnoreRadialImpulse;
	bIgnoreRadialForce = SourceComponent->bIgnoreRadialForce;
	bApplyImpulseOnDamage = SourceComponent->bApplyImpulseOnDamage;
	bReplicatePhysicsToAutonomousProxy = SourceComponent->bReplicatePhysicsToAutonomousProxy;
	bFillCollisionUnderneathForNavmesh = SourceComponent->bFillCollisionUnderneathForNavmesh;
	AlwaysLoadOnClient = SourceComponent->AlwaysLoadOnClient;
	AlwaysLoadOnServer = SourceComponent->AlwaysLoadOnServer;
	bUseEditorCompositing = SourceComponent->bUseEditorCompositing;
	bRenderCustomDepth = SourceComponent->bRenderCustomDepth;
	bVisibleInSceneCaptureOnly = SourceComponent->bVisibleInSceneCaptureOnly;
	bHiddenInSceneCapture = SourceComponent->bHiddenInSceneCapture;
	bRayTracingFarField = SourceComponent->bRayTracingFarField;
	bHasCustomNavigableGeometry = SourceComponent->bHasCustomNavigableGeometry;
	CanCharacterStepUpOn = SourceComponent->CanCharacterStepUpOn;
	LightingChannels = SourceComponent->LightingChannels;
	RayTracingGroupId = SourceComponent->RayTracingGroupId;
	VisibilityId = SourceComponent->VisibilityId;
	CustomDepthStencilValue = SourceComponent->CustomDepthStencilValue;
	TranslucencySortPriority = SourceComponent->TranslucencySortPriority;
	TranslucencySortDistanceOffset = SourceComponent->TranslucencySortDistanceOffset;
	RuntimeVirtualTextures = SourceComponent->RuntimeVirtualTextures;
	VirtualTextureLodBias = SourceComponent->VirtualTextureLodBias;
	VirtualTextureCullMips = SourceComponent->VirtualTextureCullMips;
	VirtualTextureMinCoverage = SourceComponent->VirtualTextureMinCoverage;
	VirtualTextureRenderPassType = SourceComponent->VirtualTextureRenderPassType;
	BoundsScale = SourceComponent->BoundsScale;
	RayTracingGroupCullingPriority = SourceComponent->RayTracingGroupCullingPriority;
	CustomDepthStencilWriteMask = SourceComponent->CustomDepthStencilWriteMask;
}

void FPCGExPrimitiveComponentDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	UPrimitiveComponent* TargetComponent = InComponent;

	TargetComponent->MinDrawDistance = MinDrawDistance;
	TargetComponent->LDMaxDrawDistance = LDMaxDrawDistance;
	TargetComponent->CachedMaxDrawDistance = CachedMaxDrawDistance;
	TargetComponent->IndirectLightingCacheQuality = IndirectLightingCacheQuality;
#if PCGEX_ENGINE_VERSION < 505
	TargetComponent->LightmapType = LightmapType;
#else
	TargetComponent->SetLightmapType(LightmapType);
#endif
	TargetComponent->HLODBatchingPolicy = HLODBatchingPolicy;
	TargetComponent->bEnableAutoLODGeneration = bEnableAutoLODGeneration;
	TargetComponent->bNeverDistanceCull = bNeverDistanceCull;
	TargetComponent->BodyInstance.CopyBodyInstancePropertiesFrom(&BodyInstance);
	TargetComponent->bAlwaysCreatePhysicsState = bAlwaysCreatePhysicsState;
	TargetComponent->bMultiBodyOverlap = bMultiBodyOverlap;
	TargetComponent->bTraceComplexOnMove = bTraceComplexOnMove;
	TargetComponent->bReturnMaterialOnMove = bReturnMaterialOnMove;
	TargetComponent->bAllowCullDistanceVolume = bAllowCullDistanceVolume;
	TargetComponent->bVisibleInReflectionCaptures = bVisibleInReflectionCaptures;
	TargetComponent->bVisibleInRealTimeSkyCaptures = bVisibleInRealTimeSkyCaptures;
	TargetComponent->bVisibleInRayTracing = bVisibleInRayTracing;
	TargetComponent->bRenderInMainPass = bRenderInMainPass;
	TargetComponent->bRenderInDepthPass = bRenderInDepthPass;
	TargetComponent->bReceivesDecals = bReceivesDecals;
	TargetComponent->bHoldout = bHoldout;
	TargetComponent->bOwnerNoSee = bOwnerNoSee;
	TargetComponent->bOnlyOwnerSee = bOnlyOwnerSee;
	TargetComponent->bTreatAsBackgroundForOcclusion = bTreatAsBackgroundForOcclusion;
	TargetComponent->bUseAsOccluder = bUseAsOccluder;
	TargetComponent->bForceMipStreaming = bForceMipStreaming;
	TargetComponent->CastShadow = CastShadow;
	TargetComponent->bEmissiveLightSource = bEmissiveLightSource;
	TargetComponent->bAffectDynamicIndirectLighting = bAffectDynamicIndirectLighting;
	TargetComponent->bAffectIndirectLightingWhileHidden = bAffectIndirectLightingWhileHidden;
	TargetComponent->bAffectDistanceFieldLighting = bAffectDistanceFieldLighting;
	TargetComponent->bCastDynamicShadow = bCastDynamicShadow;
	TargetComponent->bCastStaticShadow = bCastStaticShadow;
	TargetComponent->ShadowCacheInvalidationBehavior = ShadowCacheInvalidationBehavior;
	TargetComponent->bCastVolumetricTranslucentShadow = bCastVolumetricTranslucentShadow;
	TargetComponent->bCastContactShadow = bCastContactShadow;
	TargetComponent->bSelfShadowOnly = bSelfShadowOnly;
	TargetComponent->bCastFarShadow = bCastFarShadow;
	TargetComponent->bCastInsetShadow = bCastInsetShadow;
	TargetComponent->bCastCinematicShadow = bCastCinematicShadow;
	TargetComponent->bCastHiddenShadow = bCastHiddenShadow;
	TargetComponent->bCastShadowAsTwoSided = bCastShadowAsTwoSided;
	TargetComponent->bLightAttachmentsAsGroup = bLightAttachmentsAsGroup;
	TargetComponent->bExcludeFromLightAttachmentGroup = bExcludeFromLightAttachmentGroup;
	TargetComponent->bReceiveMobileCSMShadows = bReceiveMobileCSMShadows;
	TargetComponent->bSingleSampleShadowFromStationaryLights = bSingleSampleShadowFromStationaryLights;
	TargetComponent->bIgnoreRadialImpulse = bIgnoreRadialImpulse;
	TargetComponent->bIgnoreRadialForce = bIgnoreRadialForce;
	TargetComponent->bApplyImpulseOnDamage = bApplyImpulseOnDamage;
	TargetComponent->bReplicatePhysicsToAutonomousProxy = bReplicatePhysicsToAutonomousProxy;
	TargetComponent->bFillCollisionUnderneathForNavmesh = bFillCollisionUnderneathForNavmesh;
	TargetComponent->AlwaysLoadOnClient = AlwaysLoadOnClient;
	TargetComponent->AlwaysLoadOnServer = AlwaysLoadOnServer;
	TargetComponent->bUseEditorCompositing = bUseEditorCompositing;
	TargetComponent->bRenderCustomDepth = bRenderCustomDepth;
	TargetComponent->bVisibleInSceneCaptureOnly = bVisibleInSceneCaptureOnly;
	TargetComponent->bHiddenInSceneCapture = bHiddenInSceneCapture;
	TargetComponent->bRayTracingFarField = bRayTracingFarField;
	TargetComponent->bHasCustomNavigableGeometry = bHasCustomNavigableGeometry;
	TargetComponent->CanCharacterStepUpOn = CanCharacterStepUpOn;
	TargetComponent->LightingChannels = LightingChannels;
	TargetComponent->RayTracingGroupId = RayTracingGroupId;
	TargetComponent->VisibilityId = VisibilityId;
	TargetComponent->CustomDepthStencilValue = CustomDepthStencilValue;
	TargetComponent->TranslucencySortPriority = TranslucencySortPriority;
	TargetComponent->TranslucencySortDistanceOffset = TranslucencySortDistanceOffset;
	TargetComponent->RuntimeVirtualTextures = RuntimeVirtualTextures;
	TargetComponent->VirtualTextureLodBias = VirtualTextureLodBias;
	TargetComponent->VirtualTextureCullMips = VirtualTextureCullMips;
	TargetComponent->VirtualTextureMinCoverage = VirtualTextureMinCoverage;
	TargetComponent->VirtualTextureRenderPassType = VirtualTextureRenderPassType;
	TargetComponent->BoundsScale = BoundsScale;
	TargetComponent->RayTracingGroupCullingPriority = RayTracingGroupCullingPriority;
	TargetComponent->CustomDepthStencilWriteMask = CustomDepthStencilWriteMask;
}

FPCGExMeshComponentDescriptor::FPCGExMeshComponentDescriptor()
	: FPCGExPrimitiveComponentDescriptor(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UMeshComponent::StaticClass()->GetDefaultObject<UMeshComponent>(), false);
}

void FPCGExMeshComponentDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	FPCGExPrimitiveComponentDescriptor::InitFrom(Component, bInitBodyInstance);

	const UMeshComponent* SourceComponent = Cast<UMeshComponent>(Component);
	if (!SourceComponent) { return; }

	OverrideMaterials = SourceComponent->OverrideMaterials;
	OverlayMaterial = SourceComponent->OverlayMaterial;
	OverlayMaterialMaxDrawDistance = SourceComponent->OverlayMaterialMaxDrawDistance;
}

void FPCGExMeshComponentDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	FPCGExPrimitiveComponentDescriptor::InitComponent(InComponent);

	UMeshComponent* TargetComponent = Cast<UMeshComponent>(InComponent);
	if (!TargetComponent) { return; }

	TargetComponent->OverrideMaterials = OverrideMaterials;
	TargetComponent->OverlayMaterial = OverlayMaterial;
	TargetComponent->OverlayMaterialMaxDrawDistance = OverlayMaterialMaxDrawDistance;
}

FPCGExStaticMeshComponentDescriptor::FPCGExStaticMeshComponentDescriptor()
	: FPCGExMeshComponentDescriptor(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UStaticMeshComponent::StaticClass()->GetDefaultObject<UStaticMeshComponent>(), false);
}

void FPCGExStaticMeshComponentDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	FPCGExMeshComponentDescriptor::InitFrom(Component, bInitBodyInstance);

	const UStaticMeshComponent* SourceComponent = Cast<UStaticMeshComponent>(Component);
	if (!SourceComponent) { return; }

	ForcedLodModel = SourceComponent->ForcedLodModel;
	MinLOD = SourceComponent->MinLOD;
	SubDivisionStepSize = SourceComponent->SubDivisionStepSize;
	WireframeColorOverride = SourceComponent->WireframeColorOverride;
	bForceNaniteForMasked = SourceComponent->bForceNaniteForMasked;
	bDisallowNanite = SourceComponent->bDisallowNanite;
	bForceDisableNanite = SourceComponent->bForceDisableNanite;
	bEvaluateWorldPositionOffset = SourceComponent->bEvaluateWorldPositionOffset;
	bWorldPositionOffsetWritesVelocity = SourceComponent->bWorldPositionOffsetWritesVelocity;
	bEvaluateWorldPositionOffsetInRayTracing = SourceComponent->bEvaluateWorldPositionOffsetInRayTracing;
	WorldPositionOffsetDisableDistance = SourceComponent->WorldPositionOffsetDisableDistance;
	bOverrideWireframeColor = SourceComponent->bOverrideWireframeColor;
	bOverrideMinLOD = SourceComponent->bOverrideMinLOD;
#if PCGEX_ENGINE_VERSION < 505
	bDisallowMeshPaintPerInstance = SourceComponent->bDisallowMeshPaintPerInstance;
#endif
	bIgnoreInstanceForTextureStreaming = SourceComponent->bIgnoreInstanceForTextureStreaming;
	bOverrideLightMapRes = SourceComponent->bOverrideLightMapRes;
	bCastDistanceFieldIndirectShadow = SourceComponent->bCastDistanceFieldIndirectShadow;
	bOverrideDistanceFieldSelfShadowBias = SourceComponent->bOverrideDistanceFieldSelfShadowBias;
	bUseSubDivisions = SourceComponent->bUseSubDivisions;
	bUseDefaultCollision = SourceComponent->bUseDefaultCollision;
	bGenerateOverlapEvents = SourceComponent->GetGenerateOverlapEvents();
	bSortTriangles = SourceComponent->bSortTriangles;
	bReverseCulling = SourceComponent->bReverseCulling;
	OverriddenLightMapRes = SourceComponent->OverriddenLightMapRes;
	DistanceFieldIndirectShadowMinVisibility = SourceComponent->DistanceFieldIndirectShadowMinVisibility;
	DistanceFieldSelfShadowBias = SourceComponent->DistanceFieldSelfShadowBias;
	StreamingDistanceMultiplier = SourceComponent->StreamingDistanceMultiplier;
	LightmassSettings = SourceComponent->LightmassSettings;

#if WITH_EDITOR
	bCustomOverrideVertexColorPerLOD = SourceComponent->bCustomOverrideVertexColorPerLOD;
	bDisplayNaniteFallbackMesh = SourceComponent->bDisplayNaniteFallbackMesh;
#endif
}

void FPCGExStaticMeshComponentDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	FPCGExMeshComponentDescriptor::InitComponent(InComponent);

	UStaticMeshComponent* TargetComponent = Cast<UStaticMeshComponent>(InComponent);
	if (!TargetComponent) { return; }

	TargetComponent->ForcedLodModel = ForcedLodModel;
	TargetComponent->MinLOD = MinLOD;
	TargetComponent->SubDivisionStepSize = SubDivisionStepSize;
	TargetComponent->WireframeColorOverride = WireframeColorOverride;
	TargetComponent->bForceNaniteForMasked = bForceNaniteForMasked;
	TargetComponent->bDisallowNanite = bDisallowNanite;
	TargetComponent->bForceDisableNanite = bForceDisableNanite;
	TargetComponent->bEvaluateWorldPositionOffset = bEvaluateWorldPositionOffset;
	TargetComponent->bWorldPositionOffsetWritesVelocity = bWorldPositionOffsetWritesVelocity;
	TargetComponent->bEvaluateWorldPositionOffsetInRayTracing = bEvaluateWorldPositionOffsetInRayTracing;
	TargetComponent->WorldPositionOffsetDisableDistance = WorldPositionOffsetDisableDistance;
	TargetComponent->bOverrideWireframeColor = bOverrideWireframeColor;
	TargetComponent->bOverrideMinLOD = bOverrideMinLOD;
#if PCGEX_ENGINE_VERSION < 505
	TargetComponent->bDisallowMeshPaintPerInstance = bDisallowMeshPaintPerInstance;
#endif
	TargetComponent->bIgnoreInstanceForTextureStreaming = bIgnoreInstanceForTextureStreaming;
	TargetComponent->bOverrideLightMapRes = bOverrideLightMapRes;
	TargetComponent->bCastDistanceFieldIndirectShadow = bCastDistanceFieldIndirectShadow;
	TargetComponent->bOverrideDistanceFieldSelfShadowBias = bOverrideDistanceFieldSelfShadowBias;
	TargetComponent->bUseSubDivisions = bUseSubDivisions;
	TargetComponent->bUseDefaultCollision = bUseDefaultCollision;
	TargetComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
	TargetComponent->bSortTriangles = bSortTriangles;
	TargetComponent->bReverseCulling = bReverseCulling;
	TargetComponent->OverriddenLightMapRes = OverriddenLightMapRes;
	TargetComponent->DistanceFieldIndirectShadowMinVisibility = DistanceFieldIndirectShadowMinVisibility;
	TargetComponent->DistanceFieldSelfShadowBias = DistanceFieldSelfShadowBias;
	TargetComponent->StreamingDistanceMultiplier = StreamingDistanceMultiplier;
	TargetComponent->LightmassSettings = LightmassSettings;

#if WITH_EDITOR
	TargetComponent->bCustomOverrideVertexColorPerLOD = bCustomOverrideVertexColorPerLOD;
	TargetComponent->bDisplayNaniteFallbackMesh = bDisplayNaniteFallbackMesh;
#endif
}

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExComponentDescriptors.h"
#include "PCGExMacros.h"

FPCGExPrimitiveComponentDescriptor::FPCGExPrimitiveComponentDescriptor()
	: FPCGExPrimitiveComponentDescriptor(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UPrimitiveComponent::StaticClass()->GetDefaultObject<UPrimitiveComponent>());
}

void FPCGExPrimitiveComponentDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	const UPrimitiveComponent* SourceComponent = Component;

	MinDrawDistance = SourceComponent->MinDrawDistance;
	LDMaxDrawDistance = SourceComponent->LDMaxDrawDistance;
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
	else { BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type::NoCollision); }

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
	bRenderCustomDepth = SourceComponent->bRenderCustomDepth;
	bVisibleInSceneCaptureOnly = SourceComponent->bVisibleInSceneCaptureOnly;
	bHiddenInSceneCapture = SourceComponent->bHiddenInSceneCapture;
	CanCharacterStepUpOn = SourceComponent->CanCharacterStepUpOn;
	LightingChannels = SourceComponent->LightingChannels;
	RayTracingGroupId = SourceComponent->RayTracingGroupId;
	CustomDepthStencilValue = SourceComponent->CustomDepthStencilValue;
	TranslucencySortPriority = SourceComponent->TranslucencySortPriority;
	TranslucencySortDistanceOffset = SourceComponent->TranslucencySortDistanceOffset;
	//RuntimeVirtualTextures = SourceComponent->RuntimeVirtualTextures; // TODO : Load and forward
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

	// Only update visibility if it's set to false to avoid massive overhead.
	if (!bVisible) { TargetComponent->SetVisibility(false, false); }

	TargetComponent->MinDrawDistance = MinDrawDistance;
	TargetComponent->LDMaxDrawDistance = LDMaxDrawDistance;
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
	TargetComponent->bRenderCustomDepth = bRenderCustomDepth;
	TargetComponent->bVisibleInSceneCaptureOnly = bVisibleInSceneCaptureOnly;
	TargetComponent->bHiddenInSceneCapture = bHiddenInSceneCapture;
	TargetComponent->CanCharacterStepUpOn = CanCharacterStepUpOn;
	TargetComponent->LightingChannels = LightingChannels;
	TargetComponent->RayTracingGroupId = RayTracingGroupId;
	TargetComponent->CustomDepthStencilValue = CustomDepthStencilValue;
	TargetComponent->TranslucencySortPriority = TranslucencySortPriority;
	TargetComponent->TranslucencySortDistanceOffset = TranslucencySortDistanceOffset;
	//TargetComponent->RuntimeVirtualTextures = RuntimeVirtualTextures; // TODO : Load & Forward
	TargetComponent->VirtualTextureLodBias = VirtualTextureLodBias;
	TargetComponent->VirtualTextureCullMips = VirtualTextureCullMips;
	TargetComponent->VirtualTextureMinCoverage = VirtualTextureMinCoverage;
	TargetComponent->VirtualTextureRenderPassType = VirtualTextureRenderPassType;
	TargetComponent->BoundsScale = BoundsScale;
	TargetComponent->RayTracingGroupCullingPriority = RayTracingGroupCullingPriority;
	TargetComponent->CustomDepthStencilWriteMask = CustomDepthStencilWriteMask;
}

FPCGExMeshComponentDescriptor::FPCGExMeshComponentDescriptor()
	: FPCGExMeshComponentDescriptor(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UMeshComponent::StaticClass()->GetDefaultObject<UMeshComponent>(), false);
}

void FPCGExMeshComponentDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	FPCGExPrimitiveComponentDescriptor::InitFrom(Component, bInitBodyInstance);

	const UMeshComponent* SourceComponent = Cast<UMeshComponent>(Component);
	if (!SourceComponent) { return; }

	// OverrideMaterials = SourceComponent->OverrideMaterials; // TODO!!
	OverlayMaterial = SourceComponent->OverlayMaterial;
	OverlayMaterialMaxDrawDistance = SourceComponent->OverlayMaterialMaxDrawDistance;
}

void FPCGExMeshComponentDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	FPCGExPrimitiveComponentDescriptor::InitComponent(InComponent);

	UMeshComponent* TargetComponent = Cast<UMeshComponent>(InComponent);
	if (!TargetComponent) { return; }

	//TargetComponent->OverrideMaterials = OverrideMaterials; // TODO : Load & forward
	//TargetComponent->OverlayMaterial = OverlayMaterial; // TODO : Load & forward
	TargetComponent->OverlayMaterialMaxDrawDistance = OverlayMaterialMaxDrawDistance;
}

FPCGExStaticMeshComponentDescriptor::FPCGExStaticMeshComponentDescriptor()
	: FPCGExStaticMeshComponentDescriptor(NoInit)
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
	WireframeColorOverride = SourceComponent->WireframeColorOverride;
	bForceNaniteForMasked = SourceComponent->bForceNaniteForMasked;
	bDisallowNanite = SourceComponent->bDisallowNanite;
	bEvaluateWorldPositionOffset = SourceComponent->bEvaluateWorldPositionOffset;
	bWorldPositionOffsetWritesVelocity = SourceComponent->bWorldPositionOffsetWritesVelocity;
	bEvaluateWorldPositionOffsetInRayTracing = SourceComponent->bEvaluateWorldPositionOffsetInRayTracing;
	WorldPositionOffsetDisableDistance = SourceComponent->WorldPositionOffsetDisableDistance;
	bOverrideWireframeColor = SourceComponent->bOverrideWireframeColor;
	bOverrideMinLOD = SourceComponent->bOverrideMinLOD;
#if PCGEX_ENGINE_VERSION < 505
	bDisallowMeshPaintPerInstance = SourceComponent->bDisallowMeshPaintPerInstance;
#else
	bDisallowMeshPaintPerInstance = 0;
#endif
	bIgnoreInstanceForTextureStreaming = SourceComponent->bIgnoreInstanceForTextureStreaming;
	bOverrideLightMapRes = SourceComponent->bOverrideLightMapRes;
	bCastDistanceFieldIndirectShadow = SourceComponent->bCastDistanceFieldIndirectShadow;
	bOverrideDistanceFieldSelfShadowBias = SourceComponent->bOverrideDistanceFieldSelfShadowBias;
	bUseDefaultCollision = SourceComponent->bUseDefaultCollision;
	bGenerateOverlapEvents = SourceComponent->GetGenerateOverlapEvents();
	bSortTriangles = SourceComponent->bSortTriangles;
	bReverseCulling = SourceComponent->bReverseCulling;
	OverriddenLightMapRes = SourceComponent->OverriddenLightMapRes;
	DistanceFieldIndirectShadowMinVisibility = SourceComponent->DistanceFieldIndirectShadowMinVisibility;
	DistanceFieldSelfShadowBias = SourceComponent->DistanceFieldSelfShadowBias;
	StreamingDistanceMultiplier = SourceComponent->StreamingDistanceMultiplier;
	LightmassSettings = SourceComponent->LightmassSettings;
}

void FPCGExStaticMeshComponentDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	FPCGExMeshComponentDescriptor::InitComponent(InComponent);

	UStaticMeshComponent* TargetComponent = Cast<UStaticMeshComponent>(InComponent);
	if (!TargetComponent) { return; }

	TargetComponent->ForcedLodModel = ForcedLodModel;
	TargetComponent->MinLOD = MinLOD;
	TargetComponent->WireframeColorOverride = WireframeColorOverride;
	TargetComponent->bForceNaniteForMasked = bForceNaniteForMasked;
	TargetComponent->bDisallowNanite = bDisallowNanite;
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
	TargetComponent->bUseDefaultCollision = bUseDefaultCollision;
	TargetComponent->SetGenerateOverlapEvents(bGenerateOverlapEvents);
	TargetComponent->bSortTriangles = bSortTriangles;
	TargetComponent->bReverseCulling = bReverseCulling;
	TargetComponent->OverriddenLightMapRes = OverriddenLightMapRes;
	TargetComponent->DistanceFieldIndirectShadowMinVisibility = DistanceFieldIndirectShadowMinVisibility;
	TargetComponent->DistanceFieldSelfShadowBias = DistanceFieldSelfShadowBias;
	TargetComponent->StreamingDistanceMultiplier = StreamingDistanceMultiplier;
	TargetComponent->LightmassSettings = LightmassSettings;
}

FPCGExDynamicMeshDescriptor::FPCGExDynamicMeshDescriptor()
	: FPCGExDynamicMeshDescriptor(NoInit)
{
	// Make sure we have proper defaults
	InitFrom(UDynamicMeshComponent::StaticClass()->GetDefaultObject<UDynamicMeshComponent>(), false);
}

void FPCGExDynamicMeshDescriptor::InitFrom(const UPrimitiveComponent* Component, bool bInitBodyInstance)
{
	FPCGExMeshComponentDescriptor::InitFrom(Component, bInitBodyInstance);

	const UDynamicMeshComponent* SourceComponent = Cast<UDynamicMeshComponent>(Component);
	if (!SourceComponent) { return; }

#if PCGEX_ENGINE_VERSION > 504
	DistanceFieldMode = static_cast<EPCGExDynamicMeshComponentDistanceFieldMode>(static_cast<uint8>(SourceComponent->GetDistanceFieldMode()));
#endif

	bExplicitShowWireframe = SourceComponent->bExplicitShowWireframe;
	WireframeColor = SourceComponent->WireframeColor;
	ColorMode = SourceComponent->ColorMode;
	ConstantColor = SourceComponent->ConstantColor;
	ColorSpaceMode = SourceComponent->ColorSpaceMode;
	bEnableFlatShading = SourceComponent->bEnableFlatShading;
	bEnableViewModeOverrides = SourceComponent->bEnableViewModeOverrides;
	bEnableRaytracing = SourceComponent->bEnableRaytracing;
}

void FPCGExDynamicMeshDescriptor::InitComponent(UPrimitiveComponent* InComponent) const
{
	FPCGExMeshComponentDescriptor::InitComponent(InComponent);

	UDynamicMeshComponent* TargetComponent = Cast<UDynamicMeshComponent>(InComponent);
	if (!TargetComponent) { return; }

#if PCGEX_ENGINE_VERSION > 504
	TargetComponent->SetDistanceFieldMode(static_cast<EDynamicMeshComponentDistanceFieldMode>(static_cast<uint8>(DistanceFieldMode)));
#endif

	TargetComponent->bUseAsyncCooking = bUseAsyncCooking;
	TargetComponent->bDeferCollisionUpdates = bDeferCollisionUpdates;
	TargetComponent->SetComplexAsSimpleCollisionEnabled(bEnableComplexCollision, false);

	TargetComponent->bExplicitShowWireframe = bExplicitShowWireframe;
	TargetComponent->WireframeColor = WireframeColor;
	TargetComponent->ColorMode = ColorMode;
	TargetComponent->ConstantColor = ConstantColor;
	TargetComponent->ColorSpaceMode = ColorSpaceMode;
	TargetComponent->bEnableFlatShading = bEnableFlatShading;
	TargetComponent->bEnableViewModeOverrides = bEnableViewModeOverrides;
	TargetComponent->bEnableRaytracing = bEnableRaytracing;
}

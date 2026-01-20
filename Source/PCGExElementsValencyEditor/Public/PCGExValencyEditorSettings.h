// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/UnrealType.h"

#include "PCGExValencyEditorSettings.generated.h"

/**
 * Settings for the Valency Cage Editor Mode.
 * Configure visualization colors, line thicknesses, and other display options.
 * Access via Project Settings > Plugins > PCGEx Valency Editor.
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="PCGEx | Valency"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExValencyEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPCGExValencyEditorSettings();

	/** Get the singleton settings instance */
	static const UPCGExValencyEditorSettings* Get();

	/**
	 * Check if a rebuild should proceed based on property change type.
	 * Handles debouncing of interactive changes (slider dragging) to prevent spam.
	 *
	 * @param ChangeType The property change type from FPropertyChangedEvent
	 * @return true if rebuild should proceed, false if it should be skipped
	 */
	static bool ShouldAllowRebuild(EPropertyChangeType::Type ChangeType);

	//~ Begin UDeveloperSettings Interface
	virtual FName GetContainerName() const override { return "Editor"; }
	virtual FName GetCategoryName() const override { return "Plugins"; }
	virtual FName GetSectionName() const override { return FName("PCGEx | Valency"); }
	//~ End UDeveloperSettings Interface

public:
	// ========== Connection Colors ==========

	/** Color for bidirectional connections (both cages connect to each other) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Connections")
	FLinearColor BidirectionalColor = FLinearColor(0.2f, 0.8f, 0.2f);

	/** Color for unilateral connections (only one cage connects to the other) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Connections")
	FLinearColor UnilateralColor = FLinearColor(0.0f, 0.6f, 0.6f);

	/** Color for connections to null cages (boundary markers) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Connections")
	FLinearColor NullConnectionColor = FLinearColor(0.5f, 0.15f, 0.15f);

	/** Color for orbitals with no connection */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Connections")
	FLinearColor NoConnectionColor = FLinearColor(0.6f, 0.6f, 0.6f);

	/** Color for mirror cage connection line (from mirror to source) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Connections")
	FLinearColor MirrorConnectionColor = FLinearColor(0.6f, 0.2f, 0.8f);

	// ========== Pattern Cage Colors ==========

	/** Color for pattern cage proxy connections (thin dashed lines to proxied cages) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Pattern Cages")
	FLinearColor PatternProxyColor = FLinearColor(0.6f, 0.4f, 0.8f, 0.7f);

	/** Color for pattern cage orbital connections (between pattern cages) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Pattern Cages")
	FLinearColor PatternConnectionColor = FLinearColor(0.4f, 0.8f, 1.0f);

	/** Color for pattern root cage indicator */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Pattern Cages")
	FLinearColor PatternRootColor = FLinearColor(0.4f, 1.0f, 0.4f);

	/** Color for pattern wildcard cage */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Pattern Cages")
	FLinearColor PatternWildcardColor = FLinearColor(0.8f, 0.8f, 0.4f);

	/** Color for pattern constraint-only cage (not active) */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|Pattern Cages")
	FLinearColor PatternConstraintColor = FLinearColor(0.6f, 0.6f, 0.6f);

	// ========== Other Colors ==========

	/** Default color for volume wireframes */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|General")
	FLinearColor VolumeColor = FLinearColor(0.3f, 0.3f, 0.8f, 0.3f);

	/** Warning color for cages without orbital sets */
	UPROPERTY(Config, EditAnywhere, Category = "Colors|General")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.5f, 0.0f);

	// ========== Line Thicknesses ==========

	/** Thickness of thin connection lines */
	UPROPERTY(Config, EditAnywhere, Category = "Lines", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ConnectionLineThickness = 0.5f;

	/** Thickness of orbital arrow lines */
	UPROPERTY(Config, EditAnywhere, Category = "Lines", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float OrbitalArrowThickness = 1.5f;

	/** Thickness of arrowhead lines */
	UPROPERTY(Config, EditAnywhere, Category = "Lines", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ArrowheadThickness = 2.0f;

	// ========== Arrow Geometry ==========

	/** Percentage of probe radius where thick arrows start (0.0 - 1.0) */
	UPROPERTY(Config, EditAnywhere, Category = "Geometry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArrowStartOffsetPct = 0.25f;

	/** Percentage of arrow length where the main line ends (arrowhead starts) */
	UPROPERTY(Config, EditAnywhere, Category = "Geometry", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float ArrowMainLinePct = 0.85f;

	/** Size of arrowheads in world units */
	UPROPERTY(Config, EditAnywhere, Category = "Geometry", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float ArrowheadSize = 10.0f;

	/** Size of connection arrowheads in world units */
	UPROPERTY(Config, EditAnywhere, Category = "Geometry", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float ConnectionArrowheadSize = 12.0f;

	// ========== Dashed Lines ==========

	/** Length of dash segments for dashed lines */
	UPROPERTY(Config, EditAnywhere, Category = "Dashed Lines", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float DashLength = 8.0f;

	/** Gap between dash segments */
	UPROPERTY(Config, EditAnywhere, Category = "Dashed Lines", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float DashGap = 12.0f;

	// ========== Labels ==========

	/** Show cage name labels above cages */
	UPROPERTY(Config, EditAnywhere, Category = "Labels")
	bool bShowCageLabels = true;

	/** Show orbital direction labels around cages */
	UPROPERTY(Config, EditAnywhere, Category = "Labels")
	bool bShowOrbitalLabels = true;

	/** Only show labels for selected cages (when enabled, unselected cages have no labels) */
	UPROPERTY(Config, EditAnywhere, Category = "Labels")
	bool bOnlyShowSelectedLabels = true;

	/** Label color for selected cages */
	UPROPERTY(Config, EditAnywhere, Category = "Labels")
	FLinearColor SelectedLabelColor = FLinearColor::White;

	/** Label color for unselected cages (should have lower alpha) */
	UPROPERTY(Config, EditAnywhere, Category = "Labels")
	FLinearColor UnselectedLabelColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	/** Vertical offset for cage name labels above cage center */
	UPROPERTY(Config, EditAnywhere, Category = "Labels", meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float CageLabelVerticalOffset = 50.0f;

	/** Position of orbital labels as percentage of probe radius */
	UPROPERTY(Config, EditAnywhere, Category = "Labels", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float OrbitalLabelRadiusPct = 0.45f;

	// ========== Mirror Ghost ==========

	/**
	 * Enable ghost mesh preview for mirror cages.
	 * When disabled, no ghost mesh components are created, which can help with performance.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Mirror Ghost")
	bool bEnableGhostMeshes = true;

	/**
	 * Material used for ghost mesh preview in mirror cages.
	 * All mirror cages share this single material for performance.
	 * If null, uses the plugin's default ghost material (M_ValencyAssetGhost).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Mirror Ghost", meta = (EditCondition = "bEnableGhostMeshes"))
	TSoftObjectPtr<UMaterialInterface> GhostMaterial;

	/** Get the ghost material to use (resolves soft pointer, falls back to M_ValencyAssetGhost) */
	UMaterialInterface* GetGhostMaterial() const;

	// ========== Thin Line Lengths ==========

	/** Length of thin orbital lines when connected (as percentage of probe radius) */
	UPROPERTY(Config, EditAnywhere, Category = "Geometry", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ConnectedThinLinePct = 0.5f;

	// ========== Behavior ==========

	/**
	 * When enabled, rebuilds happen continuously while dragging numeric sliders.
	 * When disabled, rebuilds only happen when the slider is released.
	 *
	 * WARNING: Enabling this with large rulesets can cause performance issues
	 * or crashes due to frequent PCG cache flushes. Only enable if you have
	 * powerful hardware and small rulesets.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Behavior")
	bool bRebuildDuringInteractiveChanges = true;

	/**
	 * When enabled, flushes the entire PCG subsystem cache before regenerating.
	 * This ensures PCG graphs see updated BondingRules data, but can cause
	 * GC spikes due to cache invalidation.
	 *
	 * Try disabling this if you experience lag spikes during Valency operations.
	 * If disabled, you may need to manually refresh PCG graphs in some cases.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Behavior", meta = (EditCondition = "bAutoRegeneratePCG"))
	bool bFlushPCGCacheOnRegenerate = true;

	/**
	 * When enabled, automatically regenerates PCG graphs after building rules.
	 * Disable this if you experience GC spikes during Valency operations.
	 * You'll need to manually regenerate PCG graphs when disabled.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Behavior")
	bool bAutoRegeneratePCG = true;
};

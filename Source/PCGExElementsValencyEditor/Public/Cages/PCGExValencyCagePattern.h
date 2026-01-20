// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCageBase.h"

#include "PCGExValencyCagePattern.generated.h"

class APCGExValencyCage;
class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Pattern output strategy - how matched points are processed.
 */
UENUM(BlueprintType)
enum class EPCGExPatternOutputStrategy : uint8
{
	Remove UMETA(ToolTip = "Remove matched points from main output, output to secondary pin"),
	Collapse UMETA(ToolTip = "Collapse N matched points into 1 replacement point"),
	Swap UMETA(ToolTip = "Swap matched points to different modules"),
	Annotate UMETA(ToolTip = "Annotate matched points with metadata, no removal"),
	Fork UMETA(ToolTip = "Fork matched points to separate collection for parallel processing")
};

/**
 * Transform mode for Collapse output strategy.
 */
UENUM(BlueprintType)
enum class EPCGExPatternTransformMode : uint8
{
	Centroid UMETA(ToolTip = "Compute centroid of all matched points"),
	PatternRoot UMETA(ToolTip = "Use pattern root cage's position"),
	FirstMatch UMETA(ToolTip = "Use first matched point's transform")
};

/**
 * Settings for a pattern, stored on the pattern root cage.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyPatternSettings
{
	GENERATED_BODY()

	/** Unique name for this pattern (used for identification and attribute output) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	FName PatternName;

	/** Weight for probabilistic selection among competing patterns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern", meta = (ClampMin = "0.001"))
	float Weight = 1.0f;

	/** Minimum times this pattern must be matched (0 = no minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern", meta = (ClampMin = "0"))
	int32 MinMatches = 0;

	/** Maximum times this pattern can be matched (-1 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern", meta = (ClampMin = "-1"))
	int32 MaxMatches = -1;

	/** If true, matched points are claimed exclusively (removed from main output) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	bool bExclusive = true;

	/** Output strategy for matched points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Output")
	EPCGExPatternOutputStrategy OutputStrategy = EPCGExPatternOutputStrategy::Remove;

	/** Transform computation mode for Collapse strategy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Output", meta = (EditCondition = "OutputStrategy == EPCGExPatternOutputStrategy::Collapse", EditConditionHides))
	EPCGExPatternTransformMode TransformMode = EPCGExPatternTransformMode::Centroid;

	/**
	 * Replacement asset for Collapse strategy.
	 * Can be mesh, actor blueprint, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Output", meta = (EditCondition = "OutputStrategy == EPCGExPatternOutputStrategy::Collapse", EditConditionHides))
	TSoftObjectPtr<UObject> ReplacementAsset;

	/**
	 * Module name to swap to for Swap strategy.
	 * References a module by name from the BondingRules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Output", meta = (EditCondition = "OutputStrategy == EPCGExPatternOutputStrategy::Swap", EditConditionHides))
	FName SwapToModuleName;

	/**
	 * Optional Blueprint object to execute for custom data export.
	 * Can write additional attributes during pattern replacement.
	 * Write-only access to matched point data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Advanced", AdvancedDisplay)
	TObjectPtr<UObject> CustomDataExporter;
};

/**
 * A pattern cage representing a position in a pattern topology.
 * Pattern cages proxy regular cages (don't hold assets themselves) and
 * define patterns through their orbital connections to other pattern cages.
 *
 * Connected pattern cages form a pattern. One cage is designated as the
 * "pattern root" which holds the pattern settings and identifies the pattern.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Valency Cage (Pattern)"))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyCagePattern : public APCGExValencyCageBase
{
	GENERATED_BODY()

public:
	APCGExValencyCagePattern();

	//~ Begin AActor Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	//~ Begin APCGExValencyCageBase Interface
	virtual FString GetCageDisplayName() const override;
	virtual void SetDebugComponentsVisible(bool bVisible) override;
	//~ End APCGExValencyCageBase Interface

	/** Check if this is a pattern cage (for type checking) */
	virtual bool IsPatternCage() const { return true; }

	//~ Begin APCGExValencyCageBase Interface
	virtual void DetectNearbyConnections() override;
	//~ End APCGExValencyCageBase Interface

protected:
	//~ Begin APCGExValencyCageBase Interface
	/** Pattern cages connect to other pattern cages and null cages */
	virtual bool ShouldConsiderCageForConnection(const APCGExValencyCageBase* CandidateCage) const override;
	//~ End APCGExValencyCageBase Interface

public:

	/** Get all pattern cages connected to this one (traverses orbital connections recursively) */
	TArray<APCGExValencyCagePattern*> GetConnectedPatternCages() const;

	/** Get the pattern root cage for this pattern (follows connections to find root) */
	APCGExValencyCagePattern* FindPatternRoot() const;

	/** Notify the pattern root that the network has changed (triggers bounds update) */
	void NotifyPatternNetworkChanged();

	/** Compute the bounding box encompassing all cages in this pattern */
	FBox ComputePatternBounds() const;

public:
	// ==================== Proxy Configuration ====================

	/**
	 * Regular cages that this pattern position proxies.
	 * Match succeeds if solved module matches ANY of these cages' modules.
	 * Empty = use bIsWildcard instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Proxy")
	TArray<TObjectPtr<APCGExValencyCage>> ProxiedCages;

	/**
	 * If true, this position matches any module (ignores ProxiedCages).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Proxy")
	bool bIsWildcard = false;

	/**
	 * Show ghost mesh preview of first available asset from proxied cages.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Proxy")
	bool bShowProxyGhostMesh = true;

	/**
	 * Rebuild ghost mesh component based on the first proxied cage's content.
	 * Called when ProxiedCages changes or when entering Valency mode.
	 */
	void RefreshProxyGhostMesh();

	/** Clear the ghost mesh component */
	void ClearProxyGhostMesh();

	// ==================== Pattern Role ====================

	/**
	 * If true, points matching this position are consumed by the pattern.
	 * If false, this position is a neighbor constraint only (not consumed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Role")
	bool bIsActiveInPattern = true;

	/**
	 * If true, this cage is the pattern root (holds settings, identifies pattern).
	 * Only one cage per connected pattern group should be the root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Role")
	bool bIsPatternRoot = false;

	// ==================== Pattern Settings (Root Only) ====================

	/**
	 * Pattern settings. Only used when bIsPatternRoot is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern|Settings", meta = (EditCondition = "bIsPatternRoot", EditConditionHides))
	FPCGExValencyPatternSettings PatternSettings;

protected:
	/** Sphere component for visualization and selection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DebugSphereComponent;

	/**
	 * Box component showing pattern bounds (only visible on root cage).
	 * Encompasses all cages in the connected pattern.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> PatternBoundsComponent;

	/** Ghost mesh components for proxy preview */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ProxyGhostMeshComponents;

	/** Update the pattern bounds visualization */
	void UpdatePatternBoundsVisualization();
};

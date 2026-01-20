// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyProcessor.h"
#include "Core/PCGExValencySolverOperation.h"
#include "Core/PCGExClusterFilter.h"
#include "Elements/PCGExAssetStaging.h"

#include "PCGExValencyStaging.generated.h"

class UPCGExValencySolverInstancedFactory;

namespace PCGExCollections
{
	class FPickPacker;
}

/**
 * Selection mode when multiple modules match a fixed pick name.
 */
UENUM(BlueprintType)
enum class EPCGExFixedPickSelectionMode : uint8
{
	WeightedRandom UMETA(ToolTip = "Select from matching modules using weights (deterministic)"),
	FirstMatch UMETA(ToolTip = "Select the first matching module (deterministic)"),
	BestFit UMETA(ToolTip = "Select module with best orbital configuration match")
};

/**
 * Behavior when fixed pick module doesn't fit the node's orbital configuration.
 */
UENUM(BlueprintType)
enum class EPCGExFixedPickIncompatibleBehavior : uint8
{
	Skip UMETA(ToolTip = "Ignore fixed pick, let solver decide (with optional warning)"),
	Force UMETA(ToolTip = "Force the module regardless of orbital fit")
};

/**
 * Valency Staging - WFC-like asset staging for cluster nodes.
 * Uses orbital-based compatibility rules to place modules on cluster vertices.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "wfc wave function collapse valency staging", PCGExNodeLibraryDoc="valency/valency-staging"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyStagingSettings : public UPCGExValencyProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject
	virtual void PostInitProperties() override;
	//~End UObject

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyStaging, "Valency : Staging", "WFC-like asset staging on cluster vertices using orbital-based compatibility rules.");

	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	// This node requires both OrbitalSet and BondingRules
	virtual bool WantsOrbitalSet() const override { return true; }
	virtual bool WantsBondingRules() const override { return true; }

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** Solver algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExValencySolverInstancedFactory> Solver;

	/** If enabled, use the point's seed attribute to vary per-cluster solving */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUsePerClusterSeed = false;

	/** Output mode - determines how staging data is written */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	EPCGExStagingOutputMode OutputMode = EPCGExStagingOutputMode::CollectionMap;

	/** Attribute name for the resolved asset path output (only used with Attributes mode) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="OutputMode == EPCGExStagingOutputMode::Attributes", EditConditionHides))
	FName AssetPathAttributeName = FName("AssetPath");

	/** If enabled, output an attribute marking unsolvable nodes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputUnsolvableMarker = true;

	/** Attribute name for the unsolvable marker */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputUnsolvableMarker"))
	FName UnsolvableAttributeName = FName("bUnsolvable");

	/** If enabled, prune nodes that failed to solve */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bPruneUnsolvable = false;

	/** If enabled, output the resolved module name as an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputModuleName = false;

	/** Attribute name for the resolved module name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputModuleName"))
	FName ModuleNameAttributeName = FName("ModuleName");

	/** If enabled, applies the module's local transform offset to the point's transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bApplyLocalTransforms = true;

	// ========== Fixed Picks ==========

	/** Enable fixed picks - allows pre-assigning specific modules to vertices */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable))
	bool bEnableFixedPicks = false;

	/**
	 * Attribute containing module names for fixed picks.
	 * Vertices with a valid module name will be pre-assigned before solving.
	 * Empty attribute = no fixed picks.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks"))
	FPCGAttributePropertyInputSelector FixedPickAttribute;

	/** How to select when multiple modules match the fixed pick name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks"))
	EPCGExFixedPickSelectionMode FixedPickSelectionMode = EPCGExFixedPickSelectionMode::WeightedRandom;

	/** Behavior when fixed pick module doesn't fit the node's orbital configuration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks"))
	EPCGExFixedPickIncompatibleBehavior IncompatibleFixedPickBehavior = EPCGExFixedPickIncompatibleBehavior::Skip;

	/** Warn when fixed pick name doesn't match any module */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks"))
	bool bWarnOnUnmatchedFixedPick = false;

	/** Warn when fixed pick module doesn't fit orbital configuration (only when Skip behavior) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks && IncompatibleFixedPickBehavior == EPCGExFixedPickIncompatibleBehavior::Skip"))
	bool bWarnOnIncompatibleFixedPick = true;

	/** Default filter value when no fixed pick filters are connected (true = all points eligible) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fixed Picks", meta=(PCG_Overridable, EditCondition="bEnableFixedPicks"))
	bool bDefaultFixedPickFilterValue = true;
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyStagingContext final : FPCGExValencyProcessorContext
{
	friend class FPCGExValencyStagingElement;

	virtual void RegisterAssetDependencies() override;

	/** Solver factory (registered from settings) */
	UPCGExValencySolverInstancedFactory* Solver = nullptr;

	/** Pick packer for collection entry hash writing (shared across all batches) */
	TSharedPtr<PCGExCollections::FPickPacker> PickPacker;

	UPCGExMeshCollection* MeshCollection = nullptr;
	UPCGExActorCollection* ActorCollection = nullptr;

	/** Fixed pick filter factories (optional, controls which points are eligible for fixed picking) */
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FixedPickFilterFactories;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyStagingElement final : public FPCGExValencyProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValencyStaging
{
	class FBatch;

	class FProcessor final : public PCGExValencyMT::TProcessor<FPCGExValencyStagingContext, UPCGExValencyStagingSettings>
	{
		friend class FBatch;

	protected:
		/** Solver instance */
		TSharedPtr<FPCGExValencySolverOperation> Solver;

		/** Solver allocations (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<PCGExValency::FSolverAllocations> SolverAllocations;

		/** Attribute writers (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataWriter;
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> UnsolvableWriter;
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashWriter;
		TSharedPtr<PCGExData::TBuffer<FName>> ModuleNameWriter;

		/** Fixed pick reader (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<PCGExData::TBuffer<FName>> FixedPickReader;

		/** Fixed pick filter cache (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<TArray<int8>> FixedPickFilterCache;

		/** Fixed pick filter factories (forwarded from batch) */
		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* FixedPickFilterFactories = nullptr;

		/** Fixed pick filter manager (created in Process) */
		TSharedPtr<PCGExClusterFilter::FManager> FixedPickFiltersManager;

		/** Solve result */
		PCGExValency::FSolveResult SolveResult;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;
		virtual void Write() override;

	protected:
		/** Apply fixed picks before solver runs */
		void ApplyFixedPicks();

		/** Run the solver */
		void RunSolver();

		/** Write results to point attributes */
		void WriteResults();
	};

	class FBatch final : public PCGExValencyMT::TBatch<FProcessor>
	{
		/** Attribute writers (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataWriter;
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> UnsolvableWriter;
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashWriter;
		TSharedPtr<PCGExData::TBuffer<FName>> ModuleNameWriter;

		/** Fixed pick reader (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<FName>> FixedPickReader;

		/** Fixed pick filter factories and cache (owned here, shared with processors) */
		TSharedPtr<TArray<int8>> FixedPickFilterCache;

		/** Solver allocations (created from factory, shared with processors) */
		TSharedPtr<PCGExValency::FSolverAllocations> SolverAllocations;

	public:
		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* FixedPickFilterFactories = nullptr;
		
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}

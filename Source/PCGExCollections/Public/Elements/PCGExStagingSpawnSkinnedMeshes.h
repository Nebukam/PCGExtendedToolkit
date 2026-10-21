// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGCommon.h"
#include "PCGCrc.h"
#include "Collections/PCGExSkinnedMeshCollection.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExPointFilter.h"
#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Helpers/PCGExCollectionsHelpers.h"
#include "InstanceDataPackers/PCGSkinnedMeshInstanceDataPackerBase.h"
#include "MeshSelectors/PCGSkinnedMeshDescriptor.h"
#include "MeshSelectors/PCGSkinnedMeshSelector.h"
#include "PCGExPropertyFloatPacker.h"
#include "UObject/WeakObjectPtr.h"

#include <atomic>

#include "PCGExStagingSpawnSkinnedMeshes.generated.h"

class UPCGManagedISKMComponent;

namespace PCGExMT
{
	class FTimeSlicedMainThreadLoop;
}

/**
 * Spawns instanced skinned mesh components from staged skinned mesh collection entries.
 * One component per (entry, material pick) and target actor; transforms are consumed as-is from upstream
 * staging nodes (fitting is their responsibility).
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc",
	meta=(Keywords = "spawn skinned mesh instanced staged collection", PCGExNodeLibraryDoc="staging/staging-spawn-skinned-meshes"))
class UPCGExStagingSpawnSkinnedMeshesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(StagingSpawnSkinnedMeshes, "Staging : Spawn Skinned Meshes", "Spawns instanced skinned meshes from staged collection entries.");

	virtual EPCGSettingsType GetType() const override
	{
		return EPCGSettingsType::Spawner;
	}

	virtual FLinearColor GetNodeTitleColor() const override
	{
		return PCGEX_NODE_COLOR_OPTIN_NAME(Sampling);
	}
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual void InputPinPropertiesBeforeFilters(TArray<FPCGPinProperties>& PinProperties) const override;
	PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourcePointFiltersLabel, "Filters which points spawn an instance.", PCGExFactories::PointFilters(), false)
	//~End UPCGSettings

	virtual bool IsCacheable() const override
	{
		return false;
	}

public:
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetInstancePackerType(TSubclassOf<UPCGSkinnedMeshInstanceDataPackerBase> InInstancePackerType);

	/** Staging layer this node reads staged picks from (and strips from output points). None = default layer (PCGEx/CollectionEntry); otherwise the layer name is appended (PCGEx/CollectionEntry/<layer>). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable), AdvancedDisplay)
	FName StagingLayer = NAME_None;

	FName GetEntryIdxAttributeName() const { return PCGExCollections::Labels::EntryIdxName(StagingLayer); }

	// --- Target ---

	/** Actor the components are added to. Null = the PCG component's target actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target", meta=(PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	// --- Descriptor ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Descriptor", meta=(InlineEditConditionToggle))
	bool bUseTemplateDescriptor = false;

	/** Ignores per-entry collection descriptors, pushing only the skinned asset and tags from the entry onto this descriptor. */
	UPROPERTY(EditAnywhere, Category = "Settings|Descriptor", meta=(DisplayName="Descriptor Override", EditCondition="bUseTemplateDescriptor"))
	FPCGSoftSkinnedMeshComponentDescriptor TemplateDescriptor;

	/** Spawns every component with collision disabled, whatever its descriptor says. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Descriptor", meta=(PCG_Overridable))
	bool bForceDisableCollisions = false;

	// --- Animation ---

	/** Per-instance animation index: which of the descriptor's Transform Provider animations the instance plays (the sequence
	 *  index on an Anim Sequence provider). An index past the provider's count plays animation 0. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Animation", meta=(PCG_Overridable, DisplayName="Animation Index"))
	FPCGExInputShorthandNameInteger32 AnimationIndex = FPCGExInputShorthandNameInteger32(FName("AnimationIndex"), 0);

	// --- Materials ---

	/** Applies the entry's picked material variant on the component. Off by default: Nanite skinning may not honour
	 *  per-component material overrides on this engine version. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Materials", meta=(PCG_Overridable))
	bool bApplyMaterialOverrides = false;

	// --- Custom data ---

	/** Packs per-point attributes into each instance's custom data floats. Rotators pack as 3 floats, quaternions as 4. The
	 *  built-in packers read the points directly; a Blueprint packer receives point indices as metadata entry keys. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Custom Data", meta=(DisplayName="Instance Data Packer"))
	TSubclassOf<UPCGSkinnedMeshInstanceDataPackerBase> InstanceDataPackerType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = "Settings|Custom Data", meta=(DisplayName="Instance Data Packer Settings"))
	TObjectPtr<UPCGSkinnedMeshInstanceDataPackerBase> InstanceDataPackerParameters = nullptr;

	/** Per-entry collection properties packed into the component's Custom Primitive Data. */
	// No ShowOnlyInnerProperties: it re-categorizes promoted members under THEIR own Category.
	UPROPERTY(EditAnywhere, Category = "Settings|Custom Data", meta=(DisplayName="Custom Primitive Data Layout"))
	FPCGExPackedFloatLayout CustomPrimitiveDataLayout;

#if WITH_EDITORONLY_DATA
	/** Logs the resolved layout and each entry's floats. PCGNoHash: hashing diagnostics would rebuild every component on toggle. */
	UPROPERTY(EditAnywhere, Category = "Settings|Custom Data", meta=(DisplayName="Debug Layout", PCGNoHash))
	bool bDebugCustomPrimitiveData = false;
#endif

	// --- Output ---

	/** Sets the BoundsMin and BoundsMax of each output point to the bounds of the skinned asset spawned at it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bApplyMeshBoundsToPoints = true;

	/** Forwards the input points, minus the consumed staging attribute. Off = no output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bOutputPoints = true;

	// --- Post process ---

	/** Functions called on the target actor once every component is spawned. Functions need to be parameter-less and with
	 *  "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Post Process")
	TArray<FName> PostProcessFunctionNames;

	// --- Warnings ---

	/** Suppress warnings for picks that are not skinned mesh entries, or whose asset cannot be loaded. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietInvalidEntryWarnings = true;

	/** Warns when a component this node would spawn already exists from an identical spawn this generation (same entries at
	 *  the same points), which usually means duplicated graph logic. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bWarnOnIdenticalSpawn = true;

protected:
	void RefreshInstancePacker();
};

struct FPCGExStagingSpawnSkinnedMeshesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExStagingSpawnSkinnedMeshesElement;

	TSharedPtr<PCGExCollections::FPickUnpacker> CollectionUnpacker;

	FPCGCrc DependenciesCrc;

	/** Components created or reused this run, so an abort can release them. Appended from the main-thread spawn loops only. */
	TArray<TWeakObjectPtr<UPCGManagedISKMComponent>> TouchedResources;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExStagingSpawnSkinnedMeshesElement final : public FPCGExPointsProcessorElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override
	{
		return false;
	}

protected:
	PCGEX_ELEMENT_MAIN_THREAD_ONLY(true)
	PCGEX_ELEMENT_CREATE_CONTEXT(StagingSpawnSkinnedMeshes)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
	virtual void AbortInternal(FPCGContext* Context) const override;
};

namespace PCGExStagingSpawnSkinnedMeshes
{
	/** Per-point resolved pick, written lock-free during the parallel phase (one slot per point index) */
	struct FResolvedEntry
	{
		const FPCGExSkinnedMeshCollectionEntry* Entry = nullptr;
		const UPCGExAssetCollection* Host = nullptr;
		uint64 Hash = 0;
		int16 MaterialPick = -1;
	};

	/** One future component: every instance of one (entry, material pick) in this input */
	struct FPartition
	{
		FPCGSkinnedMeshInstanceList List;
		FPCGSkinnedMeshPackedCustomData PackedCustomData;
		TArray<float> CustomPrimitiveData;
		const FPCGExSkinnedMeshCollectionEntry* Entry = nullptr;
		const UPCGExAssetCollection* Host = nullptr;
		uint64 Hash = 0;
		int16 MaterialPick = -1;
		FBox Bounds = FBox(ForceInit);
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExStagingSpawnSkinnedMeshesContext, UPCGExStagingSpawnSkinnedMeshesSettings>
	{
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashGetter;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> AnimationIndexSV;

		/** Per-point animation index, only allocated when not in Constant mode */
		TArray<int32> AnimationIndices;

		/** Pre-sized to NumPoints -- each parallel thread writes to its own index, no locks */
		TArray<FResolvedEntry> ResolvedEntries;

		/** Partition each point landed in, -1 for none. Drives the bounds write-back. */
		TArray<int32> PartitionPerPoint;

		TArray<FPartition> Partitions;

		/** Asset paths the spawn needs loaded, one set per loop scope, collapsed on OnPointsProcessingComplete */
		TSharedPtr<PCGExMT::TScopedSet<FSoftObjectPath>> ScopedUniquePaths;

		/** Hoisted out of the parallel hot path */
		bool bApplyMaterials = false;

		/** Picks that resolved to another entry type, counted across threads and reported once */
		std::atomic<int32> NumNonSkinnedPicks{0};

		/** Cached transform range for the partition build and the spawn loop */
		TConstPCGValueRange<FTransform> Transforms;
		int32 NumPoints = 0;

		/** Main thread loop over partitions */
		TSharedPtr<PCGExMT::FTimeSlicedMainThreadLoop> MainThreadLoop;

		/** Resolved on the first spawn iteration */
		AActor* TargetActor = nullptr;

		/** Set by the reuse check on the first spawn iteration: nothing is spawned, the previous components stay. */
		bool bReused = false;

		/** Components this input created, stamped with the CRC once every partition was visited */
		TArray<UPCGManagedISKMComponent*> OwnedResources;

		/** Identifies this input's components from one execution to the next. The node's CRC alone can't: every input owns
		 *  its own components. */
		FPCGCrc InputCrc;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
		virtual void CompleteWork() override;

	private:
		/** Groups the resolved points per (entry, pick), builds each partition's descriptor and instances */
		void BuildPartitions(TSet<FSoftObjectPath>& InOutPathsToLoad);

		/** Packs per-point attributes into the partition's instance custom data through the node's packer */
		void PackCustomData(FPartition& Partition) const;

		/** Native packing path for packers that expose their selectors; false when the packer must run its own PackInstances. */
		bool PacksNatively() const;

		void OnAssetsLoaded();

		void StartSpawnLoop();

		/** Game thread: resolves the target actor and takes over the components a previous execution left, when nothing changed */
		bool PrepareSpawn();
		void SpawnPartition(int32 PartitionIndex);
		void OnAllPartitionsVisited();
	};
}

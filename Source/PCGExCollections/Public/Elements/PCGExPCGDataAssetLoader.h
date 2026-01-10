// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCollectionsCommon.h"
#include "PCGExFilterCommon.h"
#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "Factories/PCGExFactories.h"
#include "Fitting/PCGExFitting.h"
#include "Helpers/PCGExCollectionsHelpers.h"

#include "PCGExPCGDataAssetLoader.generated.h"

class UPCGDataAsset;
class UPCGExPCGDataAssetCollection;
struct FPCGExPCGDataAssetCollectionEntry;

namespace PCGExPCGDataAssetLoader
{
	class FProcessor;
}

/**
 * Spawns PCGDataAsset contents onto staged points.
 * Works with data staged by the Asset Staging node using Collection Map output.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(Keywords = "spawn pcgdata asset staged", PCGExNodeLibraryDoc="assets-management/pcgdataasset-spawner"))
class UPCGExPCGDataAssetLoaderSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PCGDataAssetLoader, "PCGDataAsset Loader", "Spawns PCGDataAsset contents from staged points.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Sampling); }
	PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
#endif

protected:
	virtual bool OutputPinsCanBeDeactivated() const override { return true; }
	virtual void InputPinPropertiesBeforeFilters(TArray<FPCGPinProperties>& PinProperties) const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails = FPCGExTransformDetails(true, true);

	/** If enabled, only spawn data from the PCGDataAsset that matches these tags. Empty means all data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable))
	bool bFilterByTags = false;

	/** Tags to include. If empty, all data is included. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable, EditCondition="bFilterByTags"))
	TSet<FString> IncludeTags;

	/** Tags to exclude. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable, EditCondition="bFilterByTags"))
	TSet<FString> ExcludeTags;

	/** Which target attributes to forward on spawned data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta = (PCG_Overridable))
	FPCGExForwardDetails TargetsForwarding;

	/** If enabled, forward input data tags to spawned data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta = (PCG_Overridable))
	bool bForwardInputTags = true;

	/** Quiet warnings about missing or invalid entries */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta = (PCG_Overridable))
	bool bQuietInvalidEntryWarnings = false;
};

/**
 * Helper to track PCGDataAsset entries per point and handle async loading.
 * Similar pattern to FSocketHelper - collects during parallel, processes after.
 */
class PCGEXCOLLECTIONS_API FPCGExPCGDataAssetHelper : public TSharedFromThis<FPCGExPCGDataAssetHelper>
{
protected:
	mutable FRWLock EntriesLock;

	// Per-point entry mapping (index = point index, value = entry pointer)
	TArray<const FPCGExPCGDataAssetCollectionEntry*> PointEntries;

	// Unique entries that need loading (entry pointer -> collected paths)
	TMap<const FPCGExPCGDataAssetCollectionEntry*, FSoftObjectPath> UniqueEntries;

	// Loaded data assets (entry pointer -> loaded asset data)
	TMap<const FPCGExPCGDataAssetCollectionEntry*, TObjectPtr<UPCGDataAsset>> LoadedAssets;

	// Streamable handle for async loading
	TSharedPtr<FStreamableHandle> LoadHandle;

public:
	using FOnLoadEnd = std::function<void(const bool bSuccess)>;

	explicit FPCGExPCGDataAssetHelper(int32 NumPoints);
	~FPCGExPCGDataAssetHelper();

	/**
	 * Thread-safe: Register an entry for a point index.
	 * Called during parallel ProcessPoints.
	 */
	void Add(int32 PointIndex, const FPCGExPCGDataAssetCollectionEntry* Entry);

	/**
	 * Load all unique PCGDataAssets. Call after parallel processing completes.
	 * Blocks until loading is complete.
	 */
	void LoadAssets(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, FOnLoadEnd&& OnLoadEnd);

	/**
	 * Get loaded data asset for a point index.
	 * Call after LoadAssets() has completed.
	 */
	UPCGDataAsset* GetAssetForPoint(int32 PointIndex) const;

	/**
	 * Get the entry for a point index.
	 */
	const FPCGExPCGDataAssetCollectionEntry* GetEntryForPoint(int32 PointIndex) const;

	/**
	 * Check if a point has a valid entry.
	 */
	bool HasValidEntry(int32 PointIndex) const;

	/**
	 * Get all unique loaded assets.
	 */
	void GetUniqueAssets(TArray<TPair<const FPCGExPCGDataAssetCollectionEntry*, UPCGDataAsset*>>& OutAssets) const;
};

struct FPCGExPCGDataAssetLoaderContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPCGDataAssetLoaderElement;
	friend class PCGExPCGDataAssetLoader::FProcessor;

	TSharedPtr<PCGExCollections::FPickUnpacker> CollectionUnpacker;

	// Base instances - one per unique PCGDataAsset
	TSharedPtr<PCGExData::FPointIOCollection> BaseDataCollection;

	// Spawned data collection
	TSharedPtr<PCGExData::FPointIOCollection> SpawnedCollection;

	// Roaming data is non-spatial data
	mutable FRWLock RoamingDataLock;
	TSet<int32> UniqueRoamingData;
	TArray<FPCGTaggedData> RoamingData;

	void RegisterRoamingData(const FPCGTaggedData& InTaggedData);

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExPCGDataAssetLoaderElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PCGDataAssetLoader)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExPCGDataAssetLoader
{
	const FName SourceStagingMap = TEXT("Map");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPCGDataAssetLoaderContext, UPCGExPCGDataAssetLoaderSettings>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashGetter;

		// Helper for tracking entries and loading
		TSharedPtr<FPCGExPCGDataAssetHelper> AssetHelper;

		// Forward handler (created after facade is available)
		TSharedPtr<PCGExData::FDataForwardHandler> ForwardHandler;

		FPCGExTransformDetails TransformDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
		void OnAssetLoadComplete(const bool bSuccess);
	};
}

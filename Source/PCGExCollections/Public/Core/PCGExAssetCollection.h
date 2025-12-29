// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PCGExAssetCollectionTypes.h"
#include "PCGExAssetGrammar.h"
#include "Core/PCGExContext.h"
#include "Details/PCGExSocket.h"
#include "Details/PCGExStagingDetails.h"
#include "Fitting/PCGExFittingVariations.h"
#include "Helpers/PCGExStreamingHelpers.h"

#include "PCGExAssetCollection.generated.h"

#if WITH_EDITOR
struct FAssetData;
#endif

class UPCGExAssetCollection;

namespace PCGExAssetCollection
{
	class FCache;
	class FCategory;
	class FMicroCache;

	enum class ELoadingFlags : uint8
	{
		Default = 0,
		Recursive,
		RecursiveCollectionsOnly,
	};
}

struct FPCGExAssetCollectionEntry;

/**
 * Entry Access Result - Clean return type for polymorphic access
 */
struct PCGEXCOLLECTIONS_API FPCGExEntryAccessResult
{
	const FPCGExAssetCollectionEntry* Entry = nullptr;
	const UPCGExAssetCollection* Host = nullptr;

	FORCEINLINE operator bool() const { return Entry != nullptr; }
	FORCEINLINE bool IsValid() const { return Entry != nullptr; }

	template <typename T>
	FORCEINLINE const T* As() const { return static_cast<const T*>(Entry); }

	// Check if entry is of a specific type
	bool IsType(PCGExAssetCollection::FTypeId TypeId) const;
};

/**
 * Staging Data - Shared across all entry types
 */
USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staging Data")
struct PCGEXCOLLECTIONS_API FPCGExAssetStagingData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 InternalIndex = -1;

	UPROPERTY()
	FSoftObjectPath Path;

	/** Sockets attached to this entry. Maintained automatically, supports user-defined entries. */
	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPCGExSocket> Sockets;

	/** Cached bounds. Computed automatically. */
	UPROPERTY(VisibleAnywhere, Category = Settings)
	FBox Bounds = FBox(ForceInit);

	template <typename T>
	T* LoadSync() const
	{
		TSoftObjectPtr<T> SoftObjectPtr = TSoftObjectPtr<T>(Path);
		PCGExHelpers::LoadBlocking_AnyThreadTpl<T>(SoftObjectPtr);
		return SoftObjectPtr.Get();
	}

	template <typename T>
	T* TryGet() const { return TSoftObjectPtr<T>(Path).Get(); }

	bool FindSocket(FName InName, const FPCGExSocket*& OutSocket) const;
	bool FindSocket(FName InName, const FString& Tag, const FPCGExSocket*& OutSocket) const;
};

/**
 * Base Collection Entry
 */
USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Collection Entry")
struct PCGEXCOLLECTIONS_API FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	virtual ~FPCGExAssetCollectionEntry() = default;
	FPCGExAssetCollectionEntry() = default;

	/** Get the type ID of this entry */
	virtual PCGExAssetCollection::FTypeId GetTypeId() const
	{
		return bIsSubCollection ? PCGExAssetCollection::TypeIds::Base : PCGExAssetCollection::TypeIds::None;
	}

	/** Check if this entry is of a specific type (or derives from it) */
	bool IsType(PCGExAssetCollection::FTypeId TypeId) const
	{
		return PCGExAssetCollection::FTypeRegistry::Get().IsA(GetTypeId(), TypeId);
	}

	// Core Properties

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1, ClampMin=0, UIMin=0))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bIsSubCollection = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExEntryVariationMode VariationMode = EPCGExEntryVariationMode::Local;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variations", EditCondition="!bIsSubCollection && VariationMode == EPCGExEntryVariationMode::Local", EditConditionHides, ShowOnlyInnerProperties))
	FPCGExFittingVariations Variations;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSet<FName> Tags;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExEntryVariationMode GrammarSource = EPCGExEntryVariationMode::Local;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FPCGExAssetGrammarDetails AssetGrammar;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	EPCGExGrammarSubCollectionMode SubGrammarMode = EPCGExGrammarSubCollectionMode::Inherit;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection && SubGrammarMode == EPCGExGrammarSubCollectionMode::Override", EditConditionHides))
	FPCGExCollectionGrammarDetails CollectionGrammar;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FPCGExAssetStagingData Staging;

	/** Internal subcollection reference - set via EDITOR_Sanitize from typed SubCollection property */
	UPROPERTY()
	TObjectPtr<UPCGExAssetCollection> InternalSubCollection;


	// Subcollection Access (Virtual - Override in derived types)

	/** Get subcollection as base type. Override in derived classes. */
	virtual const UPCGExAssetCollection* GetSubCollectionPtr() const { return InternalSubCollection; }

	/** Clear subcollection references. Override to also clear typed pointer. */
	virtual void ClearSubCollection() { InternalSubCollection = nullptr; }

	/** Check if this is a valid subcollection entry */
	bool HasValidSubCollection() const { return bIsSubCollection && GetSubCollectionPtr() != nullptr; }


	// Typed Subcollection Access (Templates for convenience)

	template <typename T>
	T* GetSubCollection() { return Cast<T>(InternalSubCollection); }

	template <typename T>
	const T* GetSubCollection() const { return Cast<T>(InternalSubCollection); }


	// Variations & Grammar

	const FPCGExFittingVariations& GetVariations(const UPCGExAssetCollection* ParentCollection) const;
	double GetGrammarSize(const UPCGExAssetCollection* Host) const;
	double GetGrammarSize(const UPCGExAssetCollection* Host, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const;
	bool FixModuleInfos(const UPCGExAssetCollection* Host, FPCGSubdivisionSubmodule& OutModule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;


	// Lifecycle

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection);
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive);
	virtual void SetAssetPath(const FSoftObjectPath& InPath);
	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize();
#endif


	// MicroCache (Per-entry cached data, e.g., material variants)

	TSharedPtr<PCGExAssetCollection::FMicroCache> MicroCache;
	virtual void BuildMicroCache();

protected:
	void ClearManagedSockets();
};

namespace PCGExAssetCollection
{
	/**
	 * Unified MicroCache base class
	 * Handles weighted random picking for per-entry sub-selections (e.g., material variants)
	 */
	class PCGEXCOLLECTIONS_API FMicroCache : public TSharedFromThis<FMicroCache>
	{
	protected:
		double WeightSum = 0;
		TArray<int32> Weights;
		TArray<int32> Order;

	public:
		FMicroCache() = default;
		virtual ~FMicroCache() = default;

		virtual FTypeId GetTypeId() const { return TypeIds::None; }

		bool IsEmpty() const { return Order.IsEmpty(); }
		int32 Num() const { return Order.Num(); }

		int32 GetPick(int32 Index, EPCGExIndexPickMode PickMode) const;
		int32 GetPickAscending(int32 Index) const;
		int32 GetPickDescending(int32 Index) const;
		int32 GetPickWeightAscending(int32 Index) const;
		int32 GetPickWeightDescending(int32 Index) const;
		int32 GetPickRandom(int32 Seed) const;
		int32 GetPickRandomWeighted(int32 Seed) const;

	protected:
		/** Initialize from weight array. Call from derived class. */
		void BuildFromWeights(TConstArrayView<int32> InWeights);
	};

	/**
	 * Category - groups entries by name for category-based picking
	 */
	class PCGEXCOLLECTIONS_API FCategory : public TSharedFromThis<FCategory>
	{
	public:
		FName Name = NAME_None;
		double WeightSum = 0;
		TArray<int32> Indices;
		TArray<int32> Weights;
		TArray<int32> Order;
		TArray<const FPCGExAssetCollectionEntry*> Entries;

		FCategory() = default;

		explicit FCategory(FName InName)
			: Name(InName)
		{
		}

		~FCategory() = default;

		FORCEINLINE bool IsEmpty() const { return Order.IsEmpty(); }
		FORCEINLINE int32 Num() const { return Order.Num(); }

		int32 GetPick(int32 Index, EPCGExIndexPickMode PickMode) const;
		int32 GetPickAscending(int32 Index) const;
		int32 GetPickDescending(int32 Index) const;
		int32 GetPickWeightAscending(int32 Index) const;
		int32 GetPickWeightDescending(int32 Index) const;
		int32 GetPickRandom(int32 Seed) const;
		int32 GetPickRandomWeighted(int32 Seed) const;

		void Reserve(int32 InNum);
		void Shrink();
		void RegisterEntry(int32 Index, const FPCGExAssetCollectionEntry* InEntry);
		void Compile();
	};

	/**
	 * Main cache - holds the main category and named sub-categories
	 */
	class PCGEXCOLLECTIONS_API FCache : public TSharedFromThis<FCache>
	{
	public:
		int32 WeightSum = 0;
		TSharedPtr<FCategory> Main;
		TMap<FName, TSharedPtr<FCategory>> Categories;

		FCache();
		~FCache() = default;

		FORCEINLINE bool IsEmpty() const { return Main ? Main->IsEmpty() : true; }

		void Compile();
		void RegisterEntry(int32 Index, const FPCGExAssetCollectionEntry* InEntry);
	};
}

/**
 * Base Asset Collection
 */
UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Asset Collection")
class PCGEXCOLLECTIONS_API UPCGExAssetCollection : public UDataAsset
{
	mutable FRWLock CacheLock;

	GENERATED_BODY()

	friend struct FPCGExAssetCollectionEntry;

public:
#pragma region Type

	/** Get the type ID of this collection */
	virtual PCGExAssetCollection::FTypeId GetTypeId() const { return PCGExAssetCollection::TypeIds::Base; }

	/** Check if this collection is of a specific type */
	bool IsType(PCGExAssetCollection::FTypeId TypeId) const
	{
		return PCGExAssetCollection::FTypeRegistry::Get().IsA(GetTypeId(), TypeId);
	}

#pragma endregion

#pragma region Cache

	PCGExAssetCollection::FCache* LoadCache();
	virtual void InvalidateCache();
	virtual void BuildCache();

#pragma endregion

#pragma region API

	/** Get entry at cache-adjusted index */
	FPCGExEntryAccessResult GetEntryAt(int32 Index) const;

	/** Get entry by index with pick mode */
	FPCGExEntryAccessResult GetEntry(int32 Index, int32 Seed, EPCGExIndexPickMode PickMode) const;

	/** Get random entry (uniform distribution) */
	FPCGExEntryAccessResult GetEntryRandom(int32 Seed) const;

	/** Get random entry (weighted by entry Weight property) */
	FPCGExEntryAccessResult GetEntryWeightedRandom(int32 Seed) const;

	// With tag inheritance
	FPCGExEntryAccessResult GetEntryAt(int32 Index, uint8 TagInheritance, TSet<FName>& OutTags) const;
	FPCGExEntryAccessResult GetEntry(int32 Index, int32 Seed, EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags) const;
	FPCGExEntryAccessResult GetEntryRandom(int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) const;
	FPCGExEntryAccessResult GetEntryWeightedRandom(int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) const;

#pragma endregion

#pragma region Enumeration

	/** Check if index is valid in the entries array */
	virtual bool IsValidIndex(int32 InIndex) const { return false; }

	/** Get total number of entries */
	virtual int32 NumEntries() const { return 0; }

	/** Get number of valid (non-zero weight) entries */
	virtual int32 GetValidEntryNum() { return LoadCache()->Main->Indices.Num(); }

	/** Initialize entries array to given size */
	virtual void InitNumEntries(int32 Num) PCGEX_NOT_IMPLEMENTED(InitNumEntries)

	/** ForEach iteration (const) */
	using FForEachConstEntryFunc = TFunctionRef<void(const FPCGExAssetCollectionEntry*, int32)>;

	virtual void ForEachEntry(FForEachConstEntryFunc Iterator) const
	{
	}

	/** ForEach iteration (mutable) */
	using FForEachEntryFunc = TFunctionRef<void(FPCGExAssetCollectionEntry*, int32)>;

	virtual void ForEachEntry(FForEachEntryFunc Iterator)
	{
	}

	/** Sort */
	using FSortEntriesFunc = TFunctionRef<bool(const FPCGExAssetCollectionEntry* A, const FPCGExAssetCollectionEntry* B)>;

	virtual void Sort(FSortEntriesFunc Predicate)
	{
	}

#pragma endregion

	void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, PCGExAssetCollection::ELoadingFlags Flags) const;

#pragma region Lifecycle

	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void BeginDestroy() override;

	void RebuildStagingData(bool bRecursive);
	void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

	bool HasCircularDependency(const UPCGExAssetCollection* OtherCollection) const;
	bool HasCircularDependency(TSet<const UPCGExAssetCollection*>& InReferences) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UFUNCTION()
	virtual void EDITOR_RebuildStagingData();

	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Recursive();

	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Project();

	void EDITOR_SanitizeAndRebuildStagingData(bool bRecursive);
	void EDITOR_AddBrowserSelectionTyped(const TArray<FAssetData>& InAssetData);

protected:
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData);

	void EDITOR_SetDirty()
	{
		Cache.Reset();
		bCacheNeedsRebuild = true;
		InvalidateCache();
	}
#endif

#pragma endregion

public:
	
	// Properties

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1, MultiLine))
	FString Notes;
#endif

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	TSet<FName> CollectionTags;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bAutoRebuildStaging = true;
#endif

	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalVariationMode = EPCGExGlobalVariationRule::PerEntry;

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExFittingVariations GlobalVariations;

	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalGrammarMode = EPCGExGlobalVariationRule::PerEntry;

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExAssetGrammarDetails GlobalAssetGrammar = FPCGExAssetGrammarDetails(FName("N/A"));

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExCollectionGrammarDetails CollectionGrammar;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bDoNotIgnoreInvalidEntries = false;

protected:
	// Internal - Override in derived classes

	/** Get entry at raw array index (not cache-adjusted). Must override. */
	virtual const FPCGExAssetCollectionEntry* GetEntryAtRawIndex(int32 Index) const { return nullptr; }

	/** Get mutable entry at raw array index. Must override. */
	virtual FPCGExAssetCollectionEntry* GetMutableEntryAtRawIndex(int32 Index) { return nullptr; }

	/** Build cache from entries. Call with your Entries array. */
	template <typename T>
	bool BuildCacheFromEntries(TArray<T>& InEntries);

	UPROPERTY()
	bool bCacheNeedsRebuild = true;

	TSharedPtr<PCGExAssetCollection::FCache> Cache;
};

// Template Implementation
template <typename T>
bool UPCGExAssetCollection::BuildCacheFromEntries(TArray<T>& InEntries)
{
	FWriteScopeLock WriteScopeLock(CacheLock);

	if (Cache) { return true; }

	Cache = MakeShared<PCGExAssetCollection::FCache>();
	bCacheNeedsRebuild = false;

	const int32 NumEntriesCount = InEntries.Num();
	Cache->Main->Reserve(NumEntriesCount);

	for (int32 i = 0; i < NumEntriesCount; i++)
	{
		T& Entry = InEntries[i];
		if (!Entry.Validate(this)) { continue; }

		Cache->RegisterEntry(i, static_cast<const FPCGExAssetCollectionEntry*>(&Entry));
	}

	Cache->Compile();

	return true;
}

// Boilerplate Macro

/**
 * Implements required virtual functions for a collection class.
 * Place in the class body after GENERATED_BODY()
 * 
 * Usage:
 *   UCLASS()
 *   class UMyCollection : public UPCGExAssetCollection
 *   {
 *       GENERATED_BODY()
 *       PCGEX_ASSET_COLLECTION_BODY(FMyCollectionEntry)
 *       
 *       UPROPERTY(...)
 *       TArray<FMyCollectionEntry> Entries;
 *   };
 */
#define PCGEX_ASSET_COLLECTION_BODY(_ENTRY_TYPE) \
public: \
	virtual bool IsValidIndex(int32 InIndex) const override { return Entries.IsValidIndex(InIndex); } \
	virtual int32 NumEntries() const override { return Entries.Num(); } \
	virtual void InitNumEntries(int32 InNum) override { PCGExArrayHelpers::InitArray(Entries, InNum); } \
	virtual void BuildCache() override { BuildCacheFromEntries(Entries); } \
	virtual void ForEachEntry(FForEachConstEntryFunc Iterator) const override \
	{ for (int32 i = 0; i < Entries.Num(); i++) { Iterator(&Entries[i], i); } } \
	virtual void ForEachEntry(FForEachEntryFunc Iterator) override \
	{ for (int32 i = 0; i < Entries.Num(); i++) { Iterator(&Entries[i], i); } } \
	virtual void Sort(FSortEntriesFunc Predicate) override \
	{ Entries.Sort([&](const _ENTRY_TYPE& A, const _ENTRY_TYPE& B)\
	{ return Predicate(static_cast<const FPCGExAssetCollectionEntry*>(&A), static_cast<const FPCGExAssetCollectionEntry*>(&B)); }); }\
protected: \
	virtual const FPCGExAssetCollectionEntry* GetEntryAtRawIndex(int32 Index) const override \
	{ return Entries.IsValidIndex(Index) ? &Entries[Index] : nullptr; } \
	virtual FPCGExAssetCollectionEntry* GetMutableEntryAtRawIndex(int32 Index) override \
	{ return Entries.IsValidIndex(Index) ? &Entries[Index] : nullptr; } \
public:

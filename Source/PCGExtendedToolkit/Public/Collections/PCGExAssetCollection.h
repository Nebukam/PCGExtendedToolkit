// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PCGExAssetGrammar.h"
#include "PCGExContext.h"
#include "PCGExHelpers.h"
#include "PCGExStreamingHelpers.h"
#include "Details/PCGExDetailsStaging.h"
#include "Transform/PCGExFitting.h"
#include "Transform/PCGExTransform.h"

#include "PCGExAssetCollection.generated.h"

#if WITH_EDITOR
struct FAssetData; // forward declare this
#endif

#define PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryAtTpl(this, OutTypedEntry, Entries, Index, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryTpl(this, OutTypedEntry, Entries, Index, Seed, PickMode, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryRandomTpl(this, OutTypedEntry, Entries, Seed, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryWeightedRandomTpl(this, OutTypedEntry, Entries, Seed, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryAtTpl(this, OutTypedEntry, Entries, Index, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryTpl(this, OutTypedEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryRandomTpl(this, OutTypedEntry, Entries, Seed, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(PCGExCollectionHelpers::GetEntryWeightedRandomTpl(this, OutTypedEntry, Entries, Seed, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }

#define PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryAtTpl(this, OutEntry, Entries, Index, OutHost); }\
bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryTpl(this, OutEntry, Entries, Index, Seed, PickMode, OutHost); }\
bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryRandomTpl(this, OutEntry, Entries, Seed, OutHost); }\
bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryWeightedRandomTpl(this, OutEntry, Entries, Seed, OutHost); }\
bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryAtTpl(this, OutEntry, Entries, Index, TagInheritance, OutTags, OutHost); }\
bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryTpl(this, OutEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags, OutHost); }\
bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryRandomTpl(this, OutEntry, Entries, Seed, TagInheritance, OutTags, OutHost); }\
bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return PCGExCollectionHelpers::GetEntryWeightedRandomTpl(this, OutEntry, Entries, Seed, TagInheritance, OutTags, OutHost); }

#define PCGEX_ASSET_COLLECTION_BOILERPLATE(_TYPE, _ENTRY_TYPE)\
virtual bool IsValidIndex(const int32 InIndex) const { return Entries.IsValidIndex(InIndex); }\
virtual int32 NumEntries() const override {return Entries.Num(); }\
virtual  void InitNumEntries(const int32 InNum) override{ PCGEx::InitArray(Entries, InNum); }\
PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
virtual void BuildCache() override{ Super::BuildCache(Entries); }\
virtual void ForEachEntry(FForEachConstEntryFunc&& Iterator) const override { for(int i = 0; i < Entries.Num(); i++){ Iterator(&Entries[i], i); } }\
virtual void ForEachEntry(FForEachEntryFunc&& Iterator) override { for(int i = 0; i < Entries.Num(); i++){ Iterator(&Entries[i], i); } }


class UPCGExAssetCollection;

namespace PCGExAssetCollection
{
	enum class ELoadingFlags : uint8
	{
		Default = 0,
		Recursive,
		RecursiveCollectionsOnly,
	};

	enum class EType : uint8
	{
		None         = 0,
		Collection   = 1 << 0,
		Actor        = 1 << 1,
		Mesh         = 1 << 2,
		PCGDataAsset = 1 << 3,
	};

	ENUM_CLASS_FLAGS(EType)

	class PCGEXTENDEDTOOLKIT_API FMicroCache : public TSharedFromThis<FMicroCache>
	{
		// Per-entry cache data
		// Store entry type specifics

	public:
		FMicroCache()
		{
		}

		virtual EType GetType() const { return EType::None; }
		virtual ~FMicroCache() = default;

		bool IsEmpty() const { return Num() == 0; }
		virtual int32 Num() const { return 0; }

		virtual int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const { return -1; }

		virtual int32 GetPickAscending(const int32 Index) const { return -1; }
		virtual int32 GetPickDescending(const int32 Index) const { return -1; }
		virtual int32 GetPickWeightAscending(const int32 Index) const { return -1; }
		virtual int32 GetPickWeightDescending(const int32 Index) const { return -1; }
		virtual int32 GetPickRandom(const int32 Seed) const { return -1; }
		virtual int32 GetPickRandomWeighted(const int32 Seed) const { return -1; }
	};
}

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staging Data")
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetStagingData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 InternalIndex = -1;

	UPROPERTY()
	FSoftObjectPath Path;

	/** A list of socket attached to this entry. Maintained automatically, but support user-defined entries. */
	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPCGExSocket> Sockets;

	/** The bounds of this entry. This is computed automatically and cannot be edited. */
	UPROPERTY(VisibleAnywhere, Category = Settings)
	FBox Bounds = FBox(ForceInit);

	template <typename T>
	T* LoadSync() const
	{
		// Note : we forfeit the handle here
		TSoftObjectPtr<T> SoftObjectPtr = TSoftObjectPtr<T>(Path);
		PCGExHelpers::LoadBlocking_AnyThread<T>(SoftObjectPtr);
		return SoftObjectPtr.Get();
	}

	template <typename T>
	T* TryGet() const { return TSoftObjectPtr<T>(Path).Get(); }

	bool FindSocket(const FName InName, const FPCGExSocket*& OutSocket) const;
	bool FindSocket(const FName InName, const FString& Tag, const FPCGExSocket*& OutSocket) const;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	virtual ~FPCGExAssetCollectionEntry() = default;
	FPCGExAssetCollectionEntry() = default;

	virtual PCGExAssetCollection::EType GetType() const { return bIsSubCollection ? PCGExAssetCollection::EType::Collection : PCGExAssetCollection::EType::None; }

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

	/** Grammar source. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExEntryVariationMode GrammarSource = EPCGExEntryVariationMode::Local;

	/** Grammar details. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FPCGExAssetGrammarDetails AssetGrammar;

	/** Whether to override subcollection grammar settings. TODO: Make this an enum */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	EPCGExGrammarSubCollectionMode SubGrammarMode = EPCGExGrammarSubCollectionMode::Inherit;

	/** Grammar details subcollection overrides.  */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection && SubGrammarMode == EPCGExGrammarSubCollectionMode::Override", EditConditionHides))
	FPCGExCollectionGrammarDetails CollectionGrammar;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FPCGExAssetStagingData Staging;

	UPROPERTY()
	TObjectPtr<UPCGExAssetCollection> InternalSubCollection;

	virtual UPCGExAssetCollection* GetSubCollectionVoid() const { return nullptr; }

	template <typename T>
	T* GetSubCollection() { return Cast<T>(InternalSubCollection); }

	template <typename T>
	T* GetSubCollection() const { return Cast<T>(InternalSubCollection); }

	const FPCGExFittingVariations& GetVariations(const UPCGExAssetCollection* ParentCollection) const;

	double GetGrammarSize(const UPCGExAssetCollection* Host) const;
	double GetGrammarSize(const UPCGExAssetCollection* Host, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache) const;

	bool FixModuleInfos(const UPCGExAssetCollection* Host, FPCGSubdivisionSubmodule& OutModule, TMap<const FPCGExAssetCollectionEntry*, double>* SizeCache = nullptr) const;

	virtual void ClearSubCollection() { InternalSubCollection = nullptr; }

#if WITH_EDITOR
	virtual void EDITOR_Sanitize();
#endif

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection);
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive);
	virtual void SetAssetPath(const FSoftObjectPath& InPath);

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const;

	TSharedPtr<PCGExAssetCollection::FMicroCache> MicroCache;
	virtual void BuildMicroCache();

protected:
	void ClearManagedSockets();
};

namespace PCGExAssetCollection
{
	class PCGEXTENDEDTOOLKIT_API FCategory : public TSharedFromThis<FCategory>
	{
	public:
		FName Name = NAME_None;
		double WeightSum = 0;
		TArray<int32> Indices;
		TArray<int32> Weights;
		TArray<int32> Order;
		TArray<const FPCGExAssetCollectionEntry*> Entries;

		FCategory()
		{
		}

		explicit FCategory(const FName InName)
			: Name(InName)
		{
		}

		~FCategory() = default;

		FORCEINLINE bool IsEmpty() const { return Order.IsEmpty(); }
		FORCEINLINE int32 Num() const { return Order.Num(); }

		int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const;

		int32 GetPickAscending(const int32 Index) const;
		int32 GetPickDescending(const int32 Index) const;
		int32 GetPickWeightAscending(const int32 Index) const;
		int32 GetPickWeightDescending(const int32 Index) const;
		int32 GetPickRandom(const int32 Seed) const;
		int32 GetPickRandomWeighted(const int32 Seed) const;

		void Reserve(const int32 Num);
		void Shrink();

		void RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry);
		void Compile();
	};

	class PCGEXTENDEDTOOLKIT_API FCache : public TSharedFromThis<FCache>
	{
	public:
		int32 WeightSum = 0;
		TSharedPtr<FCategory> Main;
		TMap<FName, TSharedPtr<FCategory>> Categories;

		explicit FCache();
		~FCache() = default;

		FORCEINLINE bool IsEmpty() const { return Main ? Main->IsEmpty() : true; }

		void Compile();
		void RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry);
	};

#pragma region Staging bounds update

#pragma endregion
}

UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Asset Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExAssetCollection : public UDataAsset
{
	mutable FRWLock CacheLock;

	GENERATED_BODY()

	friend struct FPCGExAssetCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	PCGExAssetCollection::FCache* LoadCache();
	virtual void InvalidateCache();

	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;

	void RebuildStagingData(const bool bRecursive);

	virtual PCGExAssetCollection::EType GetType() const { return PCGExAssetCollection::EType::None; }


	using FForEachConstEntryFunc = std::function<void (const FPCGExAssetCollectionEntry*, const int32 i)>;

	virtual void ForEachEntry(FForEachConstEntryFunc&& Iterator) const
	{
	}

	using FForEachEntryFunc = std::function<void (FPCGExAssetCollectionEntry*, const int32 i)>;

	virtual void ForEachEntry(FForEachEntryFunc&& Iterator)
	{
	}

	void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

	bool HasCircularDependency(const UPCGExAssetCollection* OtherCollection) const;
	bool HasCircularDependency(TSet<const UPCGExAssetCollection*>& InReferences) const;

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;


	/** Rebuild Staging data just for this collection. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData();

	/** Rebuild Staging data for this collection and its sub-collections, recursively. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Recursive();

	/** Rebuild Staging data for all collection within this project. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Project();

	void EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive);

	void EDITOR_AddBrowserSelectionTyped(const TArray<FAssetData>& InAssetData);

protected:
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData);
#endif

public:
	virtual void BeginDestroy() override;

#if WITH_EDITORONLY_DATA
	/** Dev notes/comments. Editor-only data.  */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1, MultiLine))
	FString Notes;
#endif

	/** Collection tags */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	TSet<FName> CollectionTags;

#if WITH_EDITORONLY_DATA
	/**  */
	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bAutoRebuildStaging = true;
#endif

	/** Global variations rule. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalVariationMode = EPCGExGlobalVariationRule::PerEntry;

	/** Global variation settings. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExFittingVariations GlobalVariations;

	/** Global grammar rule.
	 * NOTE: Symbol is still defined per-entry. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalGrammarMode = EPCGExGlobalVariationRule::PerEntry;

	/** Global Mesh Grammar details.
	 * NOTE: Symbol is still defined per-entry. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExAssetGrammarDetails GlobalAssetGrammar = FPCGExAssetGrammarDetails(FName("N/A"));

	/** Default grammar when this collection is used as a subcollection. Note that this can be superseded by the host. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExCollectionGrammarDetails CollectionGrammar;

	/** If enabled, empty mesh will still be weighted and picked as valid entries, instead of being ignored. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bDoNotIgnoreInvalidEntries = false;

	virtual int32 GetValidEntryNum() { return LoadCache()->Main->Indices.Num(); }

	virtual void BuildCache();

	virtual bool IsValidIndex(const int32 InIndex) const { return false; }
	virtual int32 NumEntries() const { return 0; }
	virtual void InitNumEntries(const int32 Num) PCGEX_NOT_IMPLEMENTED(InitNumEntries)

	virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)
	virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)
	virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)
	virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryWeightedRandom, false)

	virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)
	virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)
	virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)
	virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) PCGEX_NOT_IMPLEMENTED_RET(GetEntryWeightedRandom, false)

	void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const;

protected:
	UPROPERTY()
	bool bCacheNeedsRebuild = true;

	TSharedPtr<PCGExAssetCollection::FCache> Cache;

	template <typename T>
	bool BuildCache(TArray<T>& InEntries)
	{
		FWriteScopeLock WriteScopeLock(CacheLock);

		if (Cache) { return true; } // Cache needs to be invalidated

		Cache = MakeShared<PCGExAssetCollection::FCache>();

		bCacheNeedsRebuild = false;

		const int32 NumEntries = InEntries.Num();
		Cache->Main->Reserve(NumEntries);

		TArray<PCGExAssetCollection::FCategory*> TempCategories;

		for (int i = 0; i < NumEntries; i++)
		{
			T& Entry = InEntries[i];
			if (!Entry.Validate(this)) { continue; }

			Cache->RegisterEntry(i, static_cast<const FPCGExAssetCollectionEntry*>(&Entry));
		}

		Cache->Compile();

		return true;
	}

#if WITH_EDITOR
	void EDITOR_SetDirty()
	{
		Cache.Reset();
		bCacheNeedsRebuild = true;
		InvalidateCache();
	}
#endif
};

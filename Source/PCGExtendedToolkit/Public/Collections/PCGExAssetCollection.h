// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGExAssetGrammar.h"
#include "PCGExHelpers.h"
#include "PCGParamData.h"
#include "Details/PCGExDetailsStaging.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Transform/PCGExTransform.h"
#include "Transform/PCGExFitting.h"

#include "PCGExAssetCollection.generated.h"


#define PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryAtTpl(OutTypedEntry, Entries, Index, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryTpl(OutTypedEntry, Entries, Index, Seed, PickMode, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryRandomTpl(OutTypedEntry, Entries, Seed, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryWeightedRandomTpl(OutTypedEntry, Entries, Seed, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryAtTpl(OutTypedEntry, Entries, Index, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryTpl(OutTypedEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryRandomTpl(OutTypedEntry, Entries, Seed, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryWeightedRandomTpl(OutTypedEntry, Entries, Seed, TagInheritance, OutTags, OutHost)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }

#define PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost) { return GetEntryAtTpl(OutEntry, Entries, Index, OutHost); }\
bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost) { return GetEntryTpl(OutEntry, Entries, Index, Seed, PickMode, OutHost); }\
bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) { return GetEntryRandomTpl(OutEntry, Entries, Seed, OutHost); }\
bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost) { return GetEntryWeightedRandomTpl(OutEntry, Entries, Seed, OutHost); }\
bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return GetEntryAtTpl(OutEntry, Entries, Index, TagInheritance, OutTags, OutHost); }\
bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return GetEntryTpl(OutEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags, OutHost); }\
bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return GetEntryRandomTpl(OutEntry, Entries, Seed, TagInheritance, OutTags, OutHost); }\
bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) { return GetEntryWeightedRandomTpl(OutEntry, Entries, Seed, TagInheritance, OutTags, OutHost); }

#define PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)\
virtual bool IsValidIndex(const int32 InIndex) const { return Entries.IsValidIndex(InIndex); }\
virtual int32 NumEntries() const override {return Entries.Num(); }\
PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
virtual bool BuildFromAttributeSet(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) override \
{ return BuildFromAttributeSetTpl(this, InContext, InAttributeSet, Details, bBuildStaging); } \
virtual bool BuildFromAttributeSet(FPCGExContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) override\
{ return BuildFromAttributeSetTpl(this, InContext, InputPin, Details, bBuildStaging);}\
virtual void RebuildStagingData(const bool bRecursive) override{ for(int i = 0; i < Entries.Num(); i++){ _ENTRY_TYPE& Entry = Entries[i]; Entry.UpdateStaging(this, i, bRecursive); } Super::RebuildStagingData(bRecursive); }\
virtual void BuildCache() override{ Super::BuildCache(Entries); }\
virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const override{\
	const bool bCollectionOnly = Flags == PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly;\
	const bool bRecursive = bCollectionOnly || Flags == PCGExAssetCollection::ELoadingFlags::Recursive;\
	for (const _ENTRY_TYPE& Entry : Entries){\
		if (Entry.bIsSubCollection){ if (bRecursive || bCollectionOnly){ if (Entry.InternalSubCollection){ Entry.InternalSubCollection->GetAssetPaths(OutPaths, Flags);}} continue; }\
		if (bCollectionOnly) { continue; }\
		Entry.GetAssetPaths(OutPaths); }}

#if WITH_EDITOR
#define PCGEX_ASSET_COLLECTION_BOILERPLATE(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)\
virtual void ForEachEntry(FForEachConstEntryFunc&& Iterator) const override { for(int i = 0; i < Entries.Num(); i++){ Iterator(&Entries[i]); } }\
virtual void ForEachEntry(FForEachEntryFunc&& Iterator) override { for(int i = 0; i < Entries.Num(); i++){ Iterator(&Entries[i]); } }\
virtual void EDITOR_SortByWeightAscendingTyped() override { EDITOR_SortByWeightAscendingInternal(Entries); }\
virtual void EDITOR_SortByWeightDescendingTyped() override { EDITOR_SortByWeightDescendingInternal(Entries); }\
virtual void EDITOR_SetWeightIndexTyped() override { EDITOR_SetWeightIndexInternal(Entries); }\
virtual void EDITOR_PadWeightTyped() override { EDITOR_PadWeightInternal(Entries); }\
virtual void EDITOR_MultWeight2Typed() override { EDITOR_MultWeightInternal(Entries, 2); }\
virtual void EDITOR_MultWeight10Typed() override { EDITOR_MultWeightInternal(Entries, 10); }\
virtual void EDITOR_WeightOneTyped() override { EDITOR_WeightOneInternal(Entries); }\
virtual void EDITOR_WeightRandomTyped() override { EDITOR_WeightRandomInternal(Entries); }\
virtual void EDITOR_NormalizedWeightToSumTyped() override { EDITOR_NormalizedWeightToSumInternal(Entries); }\
virtual void EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive) override{ for(int i = 0; i < Entries.Num(); i++){ _ENTRY_TYPE& Entry = Entries[i]; Entry.EDITOR_Sanitize(); Entry.UpdateStaging(this, i, bRecursive);} }
#else
#define PCGEX_ASSET_COLLECTION_BOILERPLATE(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)
#endif

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
		None = 0,
		Actor,
		Mesh,
		PCGDataAsset,
	};

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
		return PCGExHelpers::LoadBlocking_AnyThread<T>(TSoftObjectPtr<T>(Path));
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

		explicit FCategory(const FName InName):
			Name(InName)
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

		explicit FCache()
		{
			Main = MakeShared<FCategory>(NAME_None);
		}

		~FCache()
		{
		}

		bool IsEmpty() const { return Main ? Main->IsEmpty() : true; }
		void Compile();
		void RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry);
	};

#pragma region Staging bounds update


	void GetBoundingBoxBySpawning(const TSoftClassPtr<AActor>& InActorClass, FVector& Origin, FVector& BoxExtent, const bool bOnlyCollidingComponents = true, const bool bIncludeFromChildActors = true);

	void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const TSoftClassPtr<AActor>& InActor, const bool bOnlyCollidingComponents = true, const bool bIncludeFromChildActors = true);
	void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const UStaticMesh* InMesh);

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

	virtual void RebuildStagingData(const bool bRecursive);

	virtual PCGExAssetCollection::EType GetType() const { return PCGExAssetCollection::EType::None; }

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

#if WITH_EDITOR
	bool HasCircularDependency(const UPCGExAssetCollection* OtherCollection) const;
	bool HasCircularDependency(TSet<const UPCGExAssetCollection*>& InReferences) const;

	using FForEachConstEntryFunc = std::function<void (const FPCGExAssetCollectionEntry*)>;

	virtual void ForEachEntry(FForEachConstEntryFunc&& Iterator) const
	{
	}

	using FForEachEntryFunc = std::function<void (FPCGExAssetCollectionEntry*)>;

	virtual void ForEachEntry(FForEachEntryFunc&& Iterator)
	{
	}

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Add Content Browser selection to this collection. */
	UFUNCTION()
	void EDITOR_AddBrowserSelection();

	void EDITOR_AddBrowserSelectionTyped(const TArray<FAssetData>& InAssetData);

	/** Rebuild Staging data just for this collection. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData();

	/** Rebuild Staging data for this collection and its sub-collections, recursively. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Recursive();

	/** Rebuild Staging data for all collection within this project. */
	UFUNCTION()
	virtual void EDITOR_RebuildStagingData_Project();

#pragma region Tools

	/** Sort collection by weights in ascending order. */
	UFUNCTION()
	void EDITOR_SortByWeightAscending();

	virtual void EDITOR_SortByWeightAscendingTyped();

	template <typename T>
	void EDITOR_SortByWeightAscendingInternal(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight < B.Weight; });
	}

	/** Sort collection by weights in descending order. */
	UFUNCTION()
	void EDITOR_SortByWeightDescending();

	virtual void EDITOR_SortByWeightDescendingTyped();

	template <typename T>
	void EDITOR_SortByWeightDescendingInternal(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight > B.Weight; });
	}

	/**Sort collection by weights in descending order. */
	UFUNCTION()
	void EDITOR_SetWeightIndex();

	virtual void EDITOR_SetWeightIndexTyped();

	template <typename T>
	void EDITOR_SetWeightIndexInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = i + 1; }
	}

	/** Add 1 to all weights so it's easier to weight down some assets */
	UFUNCTION()
	void EDITOR_PadWeight();

	virtual void EDITOR_PadWeightTyped();

	template <typename T>
	void EDITOR_PadWeightInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight += 1; }
	}

	/** Multiplies all weights by 2 */
	UFUNCTION()
	void EDITOR_MultWeight2();

	virtual void EDITOR_MultWeight2Typed();

	/** Multiplies all weights by 10 */
	UFUNCTION()
	void EDITOR_MultWeight10();

	virtual void EDITOR_MultWeight10Typed();

	template <typename T>
	void EDITOR_MultWeightInternal(TArray<T>& Entries, int32 Mult)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight *= Mult; }
	}

	/** Reset all weights to 100 */
	UFUNCTION()
	void EDITOR_WeightOne();

	virtual void EDITOR_WeightOneTyped();

	template <typename T>
	void EDITOR_WeightOneInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = 100; }
	}

	/** Assign random weights to items */
	UFUNCTION()
	void EDITOR_WeightRandom();

	virtual void EDITOR_WeightRandomTyped();

	template <typename T>
	void EDITOR_WeightRandomInternal(TArray<T>& Entries)
	{
		FRandomStream RandomSource(FMath::Rand());
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = RandomSource.RandRange(1, Entries.Num() * 100); }
	}

	/** Normalize weight sum to 100 */
	UFUNCTION()
	void EDITOR_NormalizedWeightToSum();

	virtual void EDITOR_NormalizedWeightToSumTyped();

	template <typename T>
	void EDITOR_NormalizedWeightToSumInternal(TArray<T>& Entries)
	{
		double Sum = 0;
		for (int i = 0; i < Entries.Num(); i++) { Sum += Entries[i].Weight; }
		for (int i = 0; i < Entries.Num(); i++)
		{
			int32& W = Entries[i].Weight;
			if (W <= 0)
			{
				W = 0;
				continue;
			}
			const double Weight = (static_cast<double>(Entries[i].Weight) / Sum) * 100;
			W = Weight;
		}
	}

#pragma endregion

	virtual void EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive);
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData);

#endif

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

	virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)

	virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)

	virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)

	virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryWeightedRandom, false)


	virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)

	virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)

	virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)

	virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryWeightedRandom, false)

	virtual bool BuildFromAttributeSet(
		FPCGExContext* InContext,
		const UPCGParamData* InAttributeSet,
		const FPCGExAssetAttributeSetDetails& Details,
		const bool bBuildStaging = false)
	PCGEX_NOT_IMPLEMENTED_RET(BuildFromAttributeSet, false)

	virtual bool BuildFromAttributeSet(
		FPCGExContext* InContext,
		const FName InputPin,
		const FPCGExAssetAttributeSetDetails& Details,
		const bool bBuildStaging = false)
	PCGEX_NOT_IMPLEMENTED_RET(BuildFromAttributeSet, false)

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const;

protected:
#pragma region GetEntry

	template <typename T>
	bool GetEntryAtTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Index,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 Pick = LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		OutEntry = &InEntries[Pick];
		OutHost = this;
		return true;
	}

	template <typename T>
	bool GetEntryTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Index,
		const int32 Seed,
		const EPCGExIndexPickMode PickMode,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		if (const T& Entry = InEntries[PickedIndex]; Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed, OutHost);
		}
		else
		{
			OutEntry = &Entry;
			OutHost = this;
		}
		return true;
	}

	template <typename T>
	bool GetEntryRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Seed,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPickRandom(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryRandom(OutEntry, Seed * 2, OutHost);
		}
		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

	template <typename T>
	bool GetEntryWeightedRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Seed,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPickRandomWeighted(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

#pragma endregion

#pragma region GetEntryWithTags

	template <typename T>
	bool GetEntryAtTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Index,
		uint8 TagInheritance,
		TSet<FName>& OutTags,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];

		if (Entry.bIsSubCollection && Entry.SubCollection && (TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }

		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

	template <typename T>
	bool GetEntryTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Index,
		const int32 Seed,
		const EPCGExIndexPickMode PickMode,
		uint8 TagInheritance,
		TSet<FName>& OutTags,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

	template <typename T>
	bool GetEntryRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Seed,
		uint8 TagInheritance,
		TSet<FName>& OutTags,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPickRandom(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

	template <typename T>
	bool GetEntryWeightedRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries,
		const int32 Seed,
		uint8 TagInheritance,
		TSet<FName>& OutTags,
		const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = LoadCache()->Main->GetPickRandomWeighted(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = this;
		return true;
	}

#pragma endregion

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

	template <typename T>
	bool BuildFromAttributeSetTpl(
		T* InCollection, FPCGExContext* InContext, const UPCGParamData* InAttributeSet,
		const FPCGExAssetAttributeSetDetails& Details,
		const bool bBuildStaging = false) const
	{
		const UPCGMetadata* Metadata = InAttributeSet->Metadata;

		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries <= 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
			return false;
		}

		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(Metadata);
		if (Infos->Attributes.IsEmpty()) { return false; }

#define PCGEX_PROCESS_ATTRIBUTE(_TYPE, _NAME, _BODY) \
		if (const PCGEx::FAttributeIdentity* _NAME = Infos->Find(Details._NAME)){ \
			if (TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Infos->Attributes[Infos->Map[_NAME->Identifier]], Metadata)){ \
				TArray<_TYPE> Values; PCGEx::InitArray(Values, NumEntries); \
				if (Accessor->GetRange<_TYPE>(Values, 0, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible)){ for (int i = 0; i < NumEntries; i++) { _BODY } } } }

		PCGEx::InitArray(InCollection->Entries, NumEntries);

		PCGEX_PROCESS_ATTRIBUTE(FSoftObjectPath, AssetPathSourceAttribute, InCollection->Entries[i].SetAssetPath(Values[i]);)
		PCGEX_PROCESS_ATTRIBUTE(double, WeightSourceAttribute, InCollection->Entries[i].Weight = Values[i];)
		PCGEX_PROCESS_ATTRIBUTE(FName, CategorySourceAttribute, InCollection->Entries[i].Category = Values[i];)

		if (bBuildStaging) { InCollection->RebuildStagingData(false); }

		return true;
	}

	template <typename T>
	bool BuildFromAttributeSetTpl(
		T* InCollection, FPCGExContext* InContext, const FName InputPin,
		const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
	{
		const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(InputPin);
		if (Inputs.IsEmpty()) { return false; }
		for (const FPCGTaggedData& InData : Inputs)
		{
			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData.Data))
			{
				return BuildFromAttributeSetTpl<T>(InCollection, InContext, ParamData, Details, bBuildStaging);
			}
		}
		return false;
	}
};

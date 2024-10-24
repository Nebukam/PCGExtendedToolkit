// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExDetailsData.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Engine/AssetManager.h"
#include "Engine/DataAsset.h"
#include "AssetStaging/PCGExFitting.h"

#include "PCGExAssetCollection.generated.h"

#define PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
FORCEINLINE virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryAtTpl(OutTypedEntry, Entries, Index)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryTpl(OutTypedEntry, Entries, Index, Seed, PickMode)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryRandomTpl(OutTypedEntry, Entries, Seed)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed) override {\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryWeightedRandomTpl(OutTypedEntry, Entries, Seed)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryAtTpl(OutTypedEntry, Entries, Index, TagInheritance, OutTags)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryTpl(OutTypedEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryRandomTpl(OutTypedEntry, Entries, Seed, TagInheritance, OutTags)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }\
FORCEINLINE virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) override{\
const _ENTRY_TYPE* OutTypedEntry = static_cast<const _ENTRY_TYPE*>(OutEntry); if(GetEntryWeightedRandomTpl(OutTypedEntry, Entries, Seed, TagInheritance, OutTags)){ OutEntry = static_cast<const FPCGExAssetCollectionEntry*>(OutTypedEntry);  return true;} return false; }

#define PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
FORCEINLINE bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index) { return GetEntryAtTpl(OutEntry, Entries, Index); }\
FORCEINLINE bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending) { return GetEntryTpl(OutEntry, Entries, Index, Seed, PickMode); }\
FORCEINLINE bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed) { return GetEntryRandomTpl(OutEntry, Entries, Seed); }\
FORCEINLINE bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed) { return GetEntryWeightedRandomTpl(OutEntry, Entries, Seed); }\
FORCEINLINE bool GetEntryAt(const _ENTRY_TYPE*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags) { return GetEntryAtTpl(OutEntry, Entries, Index, TagInheritance, OutTags); }\
FORCEINLINE bool GetEntry(const _ENTRY_TYPE*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags) { return GetEntryTpl(OutEntry, Entries, Index, Seed, PickMode, TagInheritance, OutTags); }\
FORCEINLINE bool GetEntryRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) { return GetEntryRandomTpl(OutEntry, Entries, Seed, TagInheritance, OutTags); }\
FORCEINLINE bool GetEntryWeightedRandom(const _ENTRY_TYPE*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) { return GetEntryWeightedRandomTpl(OutEntry, Entries, Seed, TagInheritance, OutTags); }

#define PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_GET_ENTRY_TYPED(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_GET_ENTRY(_TYPE, _ENTRY_TYPE)\
virtual bool BuildFromAttributeSet(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) override \
{ return BuildFromAttributeSetTpl(this, InContext, InAttributeSet, Details, bBuildStaging); } \
virtual bool BuildFromAttributeSet(FPCGExContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) override\
{ return BuildFromAttributeSetTpl(this, InContext, InputPin, Details, bBuildStaging);}\
virtual void RebuildStagingData(const bool bRecursive) override{ for (_ENTRY_TYPE& Entry : Entries) { Entry.UpdateStaging(this, bRecursive); } Super::RebuildStagingData(bRecursive); }\
virtual void BuildCache() override{ Super::BuildCache(Entries); }


#if WITH_EDITOR
#define PCGEX_ASSET_COLLECTION_BOILERPLATE(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)\
virtual void EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive) override{ for(_ENTRY_TYPE& Entry : Entries){ Entry.EDITOR_Sanitize(); Entry.UpdateStaging(this, bRecursive);} }
#else
#define PCGEX_ASSET_COLLECTION_BOILERPLATE(_TYPE, _ENTRY_TYPE)\
PCGEX_ASSET_COLLECTION_BOILERPLATE_BASE(_TYPE, _ENTRY_TYPE)
#endif

class UPCGExAssetCollection;

UENUM(BlueprintType)
enum class EPCGExCollectionSource : uint8
{
	Asset        = 0 UMETA(DisplayName = "Asset", Tooltip="..."),
	AttributeSet = 1 UMETA(DisplayName = "Attribute Set", Tooltip="..."),
};

UENUM(BlueprintType)
enum class EPCGExIndexPickMode : uint8
{
	Ascending        = 0 UMETA(DisplayName = "Collection order (Ascending)", Tooltip="..."),
	Descending       = 1 UMETA(DisplayName = "Collection order (Descending)", Tooltip="..."),
	WeightAscending  = 2 UMETA(DisplayName = "Weight (Descending)", Tooltip="..."),
	WeightDescending = 3 UMETA(DisplayName = "Weight (Ascending)", Tooltip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distribution"))
enum class EPCGExDistribution : uint8
{
	Index          = 0 UMETA(DisplayName = "Index", ToolTip="Distribution by index"),
	Random         = 1 UMETA(DisplayName = "Random", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	WeightedRandom = 2 UMETA(DisplayName = "Weighted random", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Weight Output Mode"))
enum class EPCGExWeightOutputMode : uint8
{
	NoOutput           = 0 UMETA(DisplayName = "No Output", ToolTip="Don't output weight as an attribute"),
	Raw                = 1 UMETA(DisplayName = "Raw", ToolTip="Raw integer"),
	Normalized         = 2 UMETA(DisplayName = "Normalized", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInverted = 3 UMETA(DisplayName = "Normalized (Inverted)", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),

	NormalizedToDensity         = 4 UMETA(DisplayName = "Normalized to Density", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInvertedToDensity = 5 UMETA(DisplayName = "Normalized (Inverted) to Density", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),
};

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Asset Tag Inheritance"))
enum class EPCGExAssetTagInheritance : uint8
{
	None           = 0,
	Asset          = 1 << 1 UMETA(DisplayName = "Asset"),
	Hierarchy      = 1 << 2 UMETA(DisplayName = "Hierarchy"),
	Collection     = 1 << 3 UMETA(DisplayName = "Collection"),
	RootCollection = 1 << 4 UMETA(DisplayName = "Root Collection"),
	RootAsset      = 1 << 5 UMETA(DisplayName = "Root Asset"),
};

ENUM_CLASS_FLAGS(EPCGExAssetTagInheritance)
using EPCGExAssetTagInheritanceBitmask = TEnumAsByte<EPCGExAssetTagInheritance>;

namespace PCGExAssetCollection
{
	const FName SourceAssetCollection = TEXT("AttributeSet");

#if PCGEX_ENGINE_VERSION > 503
	const TSet<EPCGMetadataTypes> SupportedPathTypes = {
		EPCGMetadataTypes::SoftObjectPath,
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};
#else
	const TSet<EPCGMetadataTypes> SupportedPathTypes = {
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};
#endif


	const TSet<EPCGMetadataTypes> SupportedWeightTypes = {
		EPCGMetadataTypes::Float,
		EPCGMetadataTypes::Double,
		EPCGMetadataTypes::Integer32,
		EPCGMetadataTypes::Integer64,
	};

	const TSet<EPCGMetadataTypes> SupportedCategoryTypes = {
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetDistributionIndexDetails
{
	GENERATED_BODY()

	FPCGExAssetDistributionIndexDetails()
	{
		if (IndexSource.GetName() == FName("@Last")) { IndexSource.Update(TEXT("$Index")); }
	}

	/** Index picking mode*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending;

	/** Index sanitization behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	/** The name of the attribute index to read index selection from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector IndexSource;

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bRemapIndexToCollectionSize = false;

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExTruncateMode TruncateRemap = EPCGExTruncateMode::None;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetTaggingDetails : public FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExAssetTaggingDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExAssetTagInheritance"))
	uint8 GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Asset);

	bool IsEnabled() const { return GrabTags != 0; }
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetDistributionDetails
{
	GENERATED_BODY()

	FPCGExAssetDistributionDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index"))
	FPCGExAssetDistributionIndexDetails IndexSettings;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable))
	int32 LocalSeed = 0;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetAttributeSetDetails
{
	GENERATED_BODY()

	FPCGExAssetAttributeSetDetails()
	{
	}

	/** Name of the attribute on the AttributeSet that contains the asset path to be staged */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName AssetPathSourceAttribute = FName("AssetPath");

	/** Name of the attribute on the AttributeSet that contains the asset weight, if any. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName WeightSourceAttribute = NAME_None;

	/** Name of the attribute on the AttributeSet that contains the asset category, if any. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName CategorySourceAttribute = NAME_None;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staging Data")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingData
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath Path;

	UPROPERTY(VisibleAnywhere, Category = Baked)
	FBox Bounds = FBox(ForceInitToZero);

	template <typename T>
	T* LoadSynchronous() const
	{
		UObject* LoadedAsset = UAssetManager::GetStreamableManager().RequestSyncLoad(Path)->GetLoadedAsset();
		if (!LoadedAsset) { return nullptr; }
		return Cast<T>(LoadedAsset);
	}

	template <typename T>
	T* TryGet() const
	{
		TSoftObjectPtr<T> SoftPtr = TSoftObjectPtr<T>(Path);
		return SoftPtr.Get();
	}
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetCollectionEntry
{
	GENERATED_BODY()
	virtual ~FPCGExAssetCollectionEntry() = default;

	FPCGExAssetCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bIsSubCollection = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSet<FName> Tags;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection"))
	FPCGExFittingVariations Variations;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection"))
	FPCGExAssetStagingData Staging;

	//UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bSubCollection", EditConditionHides))
	//TSoftObjectPtr<UPCGExDataCollection> SubCollection;

	TObjectPtr<UPCGExAssetCollection> BaseSubCollectionPtr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;
#endif

#if WITH_EDITOR
	virtual void EDITOR_Sanitize()
	{
	}
#endif
	virtual bool Validate(const UPCGExAssetCollection* ParentCollection);
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive);
	virtual void SetAssetPath(const FSoftObjectPath& InPath) PCGEX_NOT_IMPLEMENTED(SetAssetPath(const FSoftObjectPath& InPath))

protected:
	template <typename T>
	void LoadSubCollection(TSoftObjectPtr<T> SoftPtr)
	{
		BaseSubCollectionPtr = SoftPtr.LoadSynchronous();
		if (BaseSubCollectionPtr) { OnSubCollectionLoaded(); }
	}

	virtual void OnSubCollectionLoaded();
};

namespace PCGExAssetCollection
{
	enum class ELoadingFlags : uint8
	{
		Default = 0,
		Recursive,
		RecursiveCollectionsOnly,
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FCategory
	{
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

		FORCEINLINE int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const
		{
			switch (PickMode)
			{
			default:
			case EPCGExIndexPickMode::Ascending:
				return GetPickAscending(Index);
			case EPCGExIndexPickMode::Descending:
				return GetPickDescending(Index);
			case EPCGExIndexPickMode::WeightAscending:
				return GetPickWeightAscending(Index);
			case EPCGExIndexPickMode::WeightDescending:
				return GetPickWeightDescending(Index);
			}
		}

		FORCEINLINE int32 GetPickAscending(const int32 Index) const
		{
			return Indices.IsValidIndex(Index) ? Indices[Index] : -1;
		}

		FORCEINLINE int32 GetPickDescending(const int32 Index) const
		{
			return Indices.IsValidIndex(Index) ? Indices[(Indices.Num() - 1) - Index] : -1;
		}

		FORCEINLINE int32 GetPickWeightAscending(const int32 Index) const
		{
			return Order.IsValidIndex(Index) ? Indices[Order[Index]] : -1;
		}

		FORCEINLINE int32 GetPickWeightDescending(const int32 Index) const
		{
			return Order.IsValidIndex(Index) ? Indices[Order[(Order.Num() - 1) - Index]] : -1;
		}

		FORCEINLINE int32 GetPickRandom(const int32 Seed) const
		{
			return Indices[Order[FRandomStream(Seed).RandRange(0, Order.Num() - 1)]];
		}

		FORCEINLINE int32 GetPickRandomWeighted(const int32 Seed) const
		{
			const int32 Threshold = FRandomStream(Seed).RandRange(0, WeightSum - 1);
			int32 Pick = 0;
			while (Pick < Weights.Num() && Weights[Pick] < Threshold) { Pick++; }
			return Indices[Order[Pick]];
		}


		void Reserve(const int32 Num)
		{
			Indices.Reserve(Num);
			Weights.Reserve(Num);
			Order.Reserve(Num);
		}

		void Shrink()
		{
			Indices.Shrink();
			Weights.Shrink();
			Order.Shrink();
		}

		void RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry);
		void Compile();
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FCache
	{
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

		void Compile();

		void RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry);
	};

#pragma region Staging bounds update

	static void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const AActor* InActor)
	{
		if (!InActor)
		{
			InStaging.Bounds = FBox(ForceInitToZero);
			return;
		}

		FVector Origin = FVector::ZeroVector;
		FVector Extents = FVector::ZeroVector;
		InActor->GetActorBounds(true, Origin, Extents);

		InStaging.Bounds = FBoxCenterAndExtent(Origin, Extents).GetBox();
	}

	static void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const UStaticMesh* InMesh)
	{
		if (!InMesh)
		{
			InStaging.Bounds = FBox(ForceInitToZero);
			return;
		}

		InStaging.Bounds = InMesh->GetBoundingBox();
	}

#pragma endregion
}

UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Asset Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetCollection : public UDataAsset
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

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void EDITOR_RefreshDisplayNames();

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging", ShortToolTip="Rebuild Staging data just for this collection."))
	virtual void EDITOR_RebuildStagingData();

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging (Recursive)", ShortToolTip="Rebuild Staging data for this collection and its sub-collections, recursively."))
	virtual void EDITOR_RebuildStagingData_Recursive();

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging (Project)", ShortToolTip="Rebuild Staging data for all collection within this project."))
	virtual void EDITOR_RebuildStagingData_Project();

	virtual void EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive);

#endif

	virtual void BeginDestroy() override;

	/** Collection tags */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	TSet<FName> CollectionTags;

#if WITH_EDITORONLY_DATA
	/**  */
	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bAutoRebuildStaging = true;
#endif

	/** If enabled, empty mesh will still be weighted and picked as valid entries, instead of being ignored. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bDoNotIgnoreInvalidEntries = false;

	virtual int32 GetValidEntryNum() { return LoadCache()->Main->Indices.Num(); }

	virtual void BuildCache();

	FORCEINLINE virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)

	FORCEINLINE virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)

	FORCEINLINE virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)

	FORCEINLINE virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryWeightedRandom, false)


	FORCEINLINE virtual bool GetEntryAt(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryAt, false)

	FORCEINLINE virtual bool GetEntry(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntry, false)

	FORCEINLINE virtual bool GetEntryRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags)
	PCGEX_NOT_IMPLEMENTED_RET(GetEntryRandom, false)

	FORCEINLINE virtual bool GetEntryWeightedRandom(const FPCGExAssetCollectionEntry*& OutEntry, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags)
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
	FORCEINLINE bool GetEntryAtTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Index)
	{
		const int32 Pick = LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		OutEntry = &InEntries[Pick];
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode)
	{
		const int32 Pick = LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		if (const T& Entry = InEntries[Pick]; Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, Seed); }
		else { OutEntry = &Entry; }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Seed)
	{
		const T& Entry = InEntries[LoadCache()->Main->GetPickRandom(Seed)];
		if (Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetEntryRandom(OutEntry, Seed * 2); }
		else { OutEntry = &Entry; }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryWeightedRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Seed)
	{
		const T& Entry = InEntries[LoadCache()->Main->GetPickRandomWeighted(Seed)];
		if (Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, Seed * 2); }
		else { OutEntry = &Entry; }
		return true;
	}

#pragma endregion

#pragma region GetEntryWithTags

	template <typename T>
	FORCEINLINE bool GetEntryAtTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Index,
		uint8 TagInheritance, TSet<FName>& OutTags)
	{
		const int32 Pick = LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		const T& Entry = InEntries[Pick];

		if (Entry.SubCollectionPtr && (TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollectionPtr->CollectionTags); }
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }

		OutEntry = &InEntries[Pick];
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode,
		uint8 TagInheritance, TSet<FName>& OutTags)
	{
		const int32 Pick = LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		const T& Entry = InEntries[Pick];
		if (Entry.SubCollectionPtr)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollectionPtr->CollectionTags); }
			Entry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, Seed * 2);
		}
		else
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
			OutEntry = &Entry;
		}
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Seed,
		uint8 TagInheritance, TSet<FName>& OutTags)
	{
		const T& Entry = InEntries[LoadCache()->Main->GetPickRandom(Seed)];
		if (Entry.SubCollectionPtr)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollectionPtr->CollectionTags); }
			Entry.SubCollectionPtr->GetEntryRandom(OutEntry, Seed * 2);
		}
		else
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
			OutEntry = &Entry;
		}
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryWeightedRandomTpl(
		const T*& OutEntry,
		const TArray<T>& InEntries, const int32 Seed,
		uint8 TagInheritance, TSet<FName>& OutTags)
	{
		const T& Entry = InEntries[LoadCache()->Main->GetPickRandomWeighted(Seed)];
		if (Entry.SubCollectionPtr)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollectionPtr->CollectionTags); }
			Entry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, Seed * 2);
		}
		else
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
			OutEntry = &Entry;
		}
		return true;
	}

#pragma endregion

	UPROPERTY()
	bool bCacheNeedsRebuild = true;

	TUniquePtr<PCGExAssetCollection::FCache> Cache;

	template <typename T>
	bool BuildCache(TArray<T>& InEntries)
	{
		FWriteScopeLock WriteScopeLock(CacheLock);

		if (Cache) { return true; } // Cache needs to be invalidated

		Cache = MakeUnique<PCGExAssetCollection::FCache>();

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
	}
#endif

	template <typename T>
	bool BuildFromAttributeSetTpl(
		T* InCollection, FPCGExContext* InContext, const UPCGParamData* InAttributeSet,
		const FPCGExAssetAttributeSetDetails& Details,
		const bool bBuildStaging = false) const
	{
		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys;

		const UPCGMetadata* Metadata = InAttributeSet->Metadata;

		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(Metadata);
		if (Infos->Attributes.IsEmpty()) { return false; }

		const PCGEx::FAttributeIdentity* PathIdentity = Infos->Find(Details.AssetPathSourceAttribute);
		const PCGEx::FAttributeIdentity* WeightIdentity = Infos->Find(Details.WeightSourceAttribute);
		const PCGEx::FAttributeIdentity* CategoryIdentity = Infos->Find(Details.CategorySourceAttribute);

		if (!PathIdentity || !PCGExAssetCollection::SupportedPathTypes.Contains(PathIdentity->UnderlyingType))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Path attribute '{0}' is either of unsupported type or not in the metadata. Expecting SoftObjectPath/String/Name"), FText::FromName(Details.AssetPathSourceAttribute)));
			return false;
		}

		if (WeightIdentity)
		{
			if (!PCGExAssetCollection::SupportedWeightTypes.Contains(WeightIdentity->UnderlyingType))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Weight attribute '{0}' is of unsupported type. Expecting Float/Double/int32/int64"), FText::FromName(Details.WeightSourceAttribute)));
				WeightIdentity = nullptr;
			}
			else if (Details.WeightSourceAttribute.IsNone())
			{
				CategoryIdentity = nullptr;
			}
		}

		if (CategoryIdentity)
		{
			if (!PCGExAssetCollection::SupportedCategoryTypes.Contains(CategoryIdentity->UnderlyingType))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Category attribute '{0}' is of unsupported type. Expecting String/Name"), FText::FromName(Details.CategorySourceAttribute)));
				CategoryIdentity = nullptr;
			}
			else if (Details.CategorySourceAttribute.IsNone())
			{
				CategoryIdentity = nullptr;
			}
		}

#if PCGEX_ENGINE_VERSION > 503
		Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);
#else
		Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Infos->Attributes[0]); // Probably not reliable, but make 5.3 compile -_-
#endif

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
			return false;
		}

		PCGEx::InitArray(InCollection->Entries, NumEntries);

		// Path value

		auto SetEntryPath = [&](const int32 Index, const FSoftObjectPath& Path) { InCollection->Entries[Index].SetAssetPath(Path); };

#define PCGEX_FOREACH_COLLECTION_ENTRY(_TYPE, _NAME, _BODY)\
		const FPCGMetadataAttribute<_TYPE>* A = Metadata->GetConstTypedAttribute<_TYPE>(_NAME);\
		for (int i = 0; i < NumEntries; i++) { _TYPE V = A->GetValueFromItemKey(i); _BODY }


#if PCGEX_ENGINE_VERSION > 503
		if (PathIdentity->UnderlyingType == EPCGMetadataTypes::SoftObjectPath)
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FSoftObjectPath, PathIdentity->Name, { SetEntryPath(i, V); })
		}
		else
#endif
			if (PathIdentity->UnderlyingType == EPCGMetadataTypes::String)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FString, PathIdentity->Name, { SetEntryPath(i, FSoftObjectPath(V)); })
			}
			else
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FName, PathIdentity->Name, { SetEntryPath(i, FSoftObjectPath(V.ToString())); })
			}


		// Weight value
		if (WeightIdentity)
		{
#define PCGEX_ATT_TOINT32(_NAME, _TYPE)\
			if (WeightIdentity->UnderlyingType == EPCGMetadataTypes::_NAME){ \
				PCGEX_FOREACH_COLLECTION_ENTRY(_TYPE, WeightIdentity->Name, {  InCollection->Entries[i].Weight = static_cast<int32>(V); }) }

			PCGEX_ATT_TOINT32(Integer32, int32)
			else
				PCGEX_ATT_TOINT32(Double, double)
				else
					PCGEX_ATT_TOINT32(Float, float)
					else
						PCGEX_ATT_TOINT32(Integer64, int64)

#undef PCGEX_ATT_TOINT32
		}

		// Category value
		if (CategoryIdentity)
		{
			if (CategoryIdentity->UnderlyingType == EPCGMetadataTypes::String)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FString, WeightIdentity->Name, { InCollection->Entries[i].Category = FName(V); })
			}
			else if (CategoryIdentity->UnderlyingType == EPCGMetadataTypes::Name)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FName, WeightIdentity->Name, { InCollection->Entries[i].Category = V; })
			}
		}

#undef PCGEX_FOREACH_COLLECTION_ENTRY

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

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRoamingAssetCollectionDetails : public FPCGExAssetAttributeSetDetails
{
	GENERATED_BODY()

	FPCGExRoamingAssetCollectionDetails()
	{
	}

	explicit FPCGExRoamingAssetCollectionDetails(const TSubclassOf<UPCGExAssetCollection>& InAssetCollectionType):
		bSupportCustomType(false), AssetCollectionType(InAssetCollectionType)
	{
	}

	UPROPERTY()
	bool bSupportCustomType = true;

	/** Defines what type of temp collection to build from input attribute set */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportCustomType", EditConditionHides, HideEditConditionToggle))
	TSubclassOf<UPCGExAssetCollection> AssetCollectionType;

	bool Validate(FPCGExContext* InContext) const;

	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const bool bBuildStaging = false) const;

	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const FName InputPin, const bool bBuildStaging) const;
};

namespace PCGExAssetCollection
{
	template <typename C = UPCGExAssetCollection, typename A = FPCGExAssetCollectionEntry>
	struct /*PCGEXTENDEDTOOLKIT_API*/ TDistributionHelper
	{
		C* Collection = nullptr;
		FPCGExAssetDistributionDetails Details;

		TSharedPtr<PCGExData::TBuffer<int32>> IndexGetter;

		int32 MaxIndex = 0;
		double MaxInputIndex = 0;

		TArray<int32> MinCache;
		TArray<int32> MaxCache;

		TDistributionHelper(
			C* InCollection,
			const FPCGExAssetDistributionDetails& InDetails):
			Collection(InCollection),
			Details(InDetails)
		{
		}

		bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
		{
			MaxIndex = Collection->LoadCache()->Main->Order.Num() - 1;

			if (Details.Distribution == EPCGExDistribution::Index)
			{
				if (Details.IndexSettings.bRemapIndexToCollectionSize)
				{
					// Non-dynamic since we want min-max to start with :(
					IndexGetter = InDataFacade->GetBroadcaster<int32>(Details.IndexSettings.IndexSource, true);
					MaxInputIndex = IndexGetter ? static_cast<double>(IndexGetter->Max) : 0;
				}
				else
				{
					IndexGetter = InDataFacade->GetScopedBroadcaster<int32>(Details.IndexSettings.IndexSource);
				}

				if (!IndexGetter)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Invalid Index attribute used"));
					return false;
				}
			}

			return true;
		}

		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed) const
		{
			if (Details.Distribution == EPCGExDistribution::WeightedRandom)
			{
				Collection->GetEntryWeightedRandom(OutEntry, Seed);
			}
			else if (Details.Distribution == EPCGExDistribution::Random)
			{
				Collection->GetEntryRandom(OutEntry, Seed);
			}
			else
			{
				double PickedIndex = IndexGetter->Read(PointIndex);
				if (Details.IndexSettings.bRemapIndexToCollectionSize)
				{
					PickedIndex = MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex);
					switch (Details.IndexSettings.TruncateRemap)
					{
					case EPCGExTruncateMode::Round:
						PickedIndex = FMath::RoundToInt(PickedIndex);
						break;
					case EPCGExTruncateMode::Ceil:
						PickedIndex = FMath::CeilToDouble(PickedIndex);
						break;
					case EPCGExTruncateMode::Floor:
						PickedIndex = FMath::FloorToDouble(PickedIndex);
						break;
					default:
					case EPCGExTruncateMode::None:
						break;
					}
				}

				Collection->GetEntry(
					OutEntry,
					PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
					Seed, Details.IndexSettings.PickMode);
			}
		}

		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) const
		{
			if (TagInheritance == 0)
			{
				GetEntry(OutEntry, PointIndex, Seed);
				return;
			}

			if (TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::RootCollection)) { OutTags.Append(Collection->CollectionTags); }

			if (Details.Distribution == EPCGExDistribution::WeightedRandom)
			{
				Collection->GetEntryWeightedRandom(OutEntry, Seed, TagInheritance, OutTags);
			}
			else if (Details.Distribution == EPCGExDistribution::Random)
			{
				Collection->GetEntryRandom(OutEntry, Seed, TagInheritance, OutTags);
			}
			else
			{
				double PickedIndex = IndexGetter->Read(PointIndex);
				if (Details.IndexSettings.bRemapIndexToCollectionSize)
				{
					PickedIndex = MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex);
					switch (Details.IndexSettings.TruncateRemap)
					{
					case EPCGExTruncateMode::Round:
						PickedIndex = FMath::RoundToInt(PickedIndex);
						break;
					case EPCGExTruncateMode::Ceil:
						PickedIndex = FMath::CeilToDouble(PickedIndex);
						break;
					case EPCGExTruncateMode::Floor:
						PickedIndex = FMath::FloorToDouble(PickedIndex);
						break;
					default:
					case EPCGExTruncateMode::None:
						break;
					}
				}

				Collection->GetEntry(
					OutEntry,
					PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
					Seed, Details.IndexSettings.PickMode, TagInheritance, OutTags);
			}
		}
	};
}

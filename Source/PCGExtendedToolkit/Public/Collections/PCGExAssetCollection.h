// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGParamData.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Details/PCGExSettingsMacros.h"
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

UENUM()
enum class EPCGExCollectionSource : uint8
{
	Asset        = 0 UMETA(DisplayName = "Asset", Tooltip="Use a single collection reference"),
	AttributeSet = 1 UMETA(DisplayName = "Attribute Set", Tooltip="Use a single attribute set that will be converted to a dynamic collection on the fly"),
	Attribute    = 2 UMETA(DisplayName = "Path Attribute", Tooltip="Use an attribute that's a path reference to an asset collection"),
};

UENUM()
enum class EPCGExIndexPickMode : uint8
{
	Ascending        = 0 UMETA(DisplayName = "Collection order (Ascending)", Tooltip="..."),
	Descending       = 1 UMETA(DisplayName = "Collection order (Descending)", Tooltip="..."),
	WeightAscending  = 2 UMETA(DisplayName = "Weight (Descending)", Tooltip="..."),
	WeightDescending = 3 UMETA(DisplayName = "Weight (Ascending)", Tooltip="..."),
};

UENUM()
enum class EPCGExDistribution : uint8
{
	Index          = 0 UMETA(DisplayName = "Index", ToolTip="Distribution by index"),
	Random         = 1 UMETA(DisplayName = "Random", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	WeightedRandom = 2 UMETA(DisplayName = "Weighted random", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
};

UENUM()
enum class EPCGExWeightOutputMode : uint8
{
	NoOutput           = 0 UMETA(DisplayName = "No Output", ToolTip="Don't output weight as an attribute"),
	Raw                = 1 UMETA(DisplayName = "Raw", ToolTip="Raw integer"),
	Normalized         = 2 UMETA(DisplayName = "Normalized", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInverted = 3 UMETA(DisplayName = "Normalized (Inverted)", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),

	NormalizedToDensity         = 4 UMETA(DisplayName = "Normalized to Density", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInvertedToDensity = 5 UMETA(DisplayName = "Normalized (Inverted) to Density", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),
};

UENUM()
enum class EPCGExAssetTagInheritance : uint8
{
	None           = 0,
	Asset          = 1 << 1 UMETA(DisplayName = "Asset"),
	Hierarchy      = 1 << 2 UMETA(DisplayName = "Hierarchy"),
	Collection     = 1 << 3 UMETA(DisplayName = "Collection"),
	RootCollection = 1 << 4 UMETA(DisplayName = "Root Collection"),
	RootAsset      = 1 << 5 UMETA(DisplayName = "Root Asset"),
};

UENUM()
enum class EPCGExEntryVariationMode : uint8
{
	Local  = 0 UMETA(DisplayName = "Local", ToolTip="This entry defines its own variation settings. This can be overruled at the collection level."),
	Global = 1 UMETA(DisplayName = "Global", ToolTip="Uses global variation settings"),
	//LocalGlobalFactor    = 2 UMETA(DisplayName = "Local x Global", ToolTip="Uses local variation settings multiplied by global random values (within global min/max range)"),
	//LocalGlobalFactorMin = 3 UMETA(DisplayName = "Local x Global (Min)", ToolTip="Uses local variation settings multiplied by global min values"),
	//LocalGlobalFactorMax = 4 UMETA(DisplayName = "Local x Global (Max)", ToolTip="Uses local variation settings multiplied by global max values"),
};

UENUM()
enum class EPCGExGlobalVariationRule : uint8
{
	PerEntry = 0 UMETA(DisplayName = "Per Entry", ToolTip="Let the entry choose whether it's using global variations or its own settings"),
	Overrule = 1 UMETA(DisplayName = "Overrule", ToolTip="Disregard of the entry settings and enforce global settings"),
	//OverruleFactor = 2 UMETA(DisplayName = "Overrule - Multiply Entry", ToolTip="Multiply the entry setting by global random values (within min/max range)"),
	//OverruleFactorMin = 3 UMETA(DisplayName = "Overrule - Multiply Entry (Min)", ToolTip="Disregard of the entry settings and enforce global settings"),
	//OverruleFactorMax = 4 UMETA(DisplayName = "Overrule - Multiply Entry (Max)", ToolTip="Disregard of the entry settings and enforce global settings"),
};

ENUM_CLASS_FLAGS(EPCGExAssetTagInheritance)
using EPCGExAssetTagInheritanceBitmask = TEnumAsByte<EPCGExAssetTagInheritance>;

namespace PCGExAssetCollection
{
	enum class ELoadingFlags : uint8
	{
		Default = 0,
		Recursive,
		RecursiveCollectionsOnly,
	};

	const FName SourceAssetCollection = TEXT("AttributeSet");

	const TSet<EPCGMetadataTypes> SupportedPathTypes = {
		EPCGMetadataTypes::SoftObjectPath,
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};


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
	};
}


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetDistributionIndexDetails
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

	PCGEX_SETTING_VALUE_DECL(Index, int32);

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bRemapIndexToCollectionSize = false;

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExTruncateMode TruncateRemap = EPCGExTruncateMode::None;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExComponentTaggingDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bForwardInputDataTags = true;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bOutputTagsToAttributes = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bAddTagsToData = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetTaggingDetails : public FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExAssetTaggingDetails() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExAssetTagInheritance"))
	uint8 GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Asset);

	bool IsEnabled() const { return GrabTags != 0; }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetDistributionDetails
{
	GENERATED_BODY()

	FPCGExAssetDistributionDetails() = default;

	/** If enabled, will limit pick to entries flagged with a specific category. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category", meta=(PCG_Overridable))
	bool bUseCategories = false;

	/** Type of Category */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category", meta=(PCG_Overridable, EditCondition="bUseCategories", EditConditionHides))
	EPCGExInputValueType CategoryInput = EPCGExInputValueType::Constant;

	/** Attribute to read category name from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category", meta=(PCG_Overridable, DisplayName="Category (Attr)", EditCondition="bUseCategories && CategoryInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName CategoryAttribute = FName("CategoryName");

	/** Constant category value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category", meta=(PCG_Overridable, DisplayName="Category", EditCondition="bUseCategories && CategoryInput == EPCGExInputValueType::Constant", EditConditionHides))
	FName Category = FName("Category");

	PCGEX_SETTING_VALUE_DECL(Category, FName);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution == EPCGExDistribution::Index"))
	FPCGExAssetDistributionIndexDetails IndexSettings;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 LocalSeed = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketOutputDetails
{
	GENERATED_BODY()

	FPCGExSocketOutputDetails() = default;

	/** Include or exclude sockets based on their tag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExNameFiltersDetails SocketTagFilters;

	/** Include or exclude sockets based on their name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExNameFiltersDetails SocketNameFilters;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSocketName = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteSocketName"))
	FName SocketNameAttributeName = "SocketName";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSocketTag = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteSocketTag"))
	FName SocketTagAttributeName = "SocketTag";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCategory = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteCategory"))
	FName CategoryAttributeName = "Category";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAssetPath = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteAssetPath"))
	FName AssetPathAttributeName = "AssetPath";

	/** Which scale components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformScale = static_cast<uint8>(EPCGExApplySampledComponentFlags::All);

	/** Meta filter settings for socket points, as they naturally inherit from the original points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;


	bool Init(FPCGExContext* InContext);

	TArray<int32> TrScaComponents;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetAttributeSetDetails
{
	GENERATED_BODY()

	FPCGExAssetAttributeSetDetails() = default;

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

	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=0, UIMin=0))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	bool bIsSubCollection = false;

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSet<FName> Tags;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection"))
	EPCGExEntryVariationMode VariationMode = EPCGExEntryVariationMode::Local;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection"))
	FPCGExFittingVariations Variations;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection"))
	FPCGExAssetStagingData Staging;

	UPROPERTY()
	TObjectPtr<UPCGExAssetCollection> InternalSubCollection;

	template <typename T>
	T* GetSubCollection() { return Cast<T>(InternalSubCollection); }

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;
#endif

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

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void EDITOR_RefreshDisplayNames();

	/** Add Content Browser selection to this collection. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Add Selection", ShortToolTip="Add active content browser selection to this collection.", DisplayOrder=-1))
	void EDITOR_AddBrowserSelection();

	void EDITOR_AddBrowserSelectionTyped(const TArray<FAssetData>& InAssetData);

	/** Rebuild Staging data just for this collection. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging", ShortToolTip="Rebuild Staging data just for this collection.", DisplayOrder=0))
	virtual void EDITOR_RebuildStagingData();

	/** Rebuild Staging data for this collection and its sub-collections, recursively. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging (Recursive)", ShortToolTip="Rebuild Staging data for this collection and its sub-collections, recursively.", DisplayOrder=1))
	virtual void EDITOR_RebuildStagingData_Recursive();

	/** Rebuild Staging data for all collection within this project. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Rebuild Staging (Project)", ShortToolTip="Rebuild Staging data for all collection within this project.", DisplayOrder=2))
	virtual void EDITOR_RebuildStagingData_Project();

#pragma region Tools

	/** Sort collection by weights in ascending order. */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="⇅ Ascending", ShortToolTip="Sort collection by weights in ascending order.", DisplayOrder=10))
	void EDITOR_SortByWeightAscending();

	virtual void EDITOR_SortByWeightAscendingTyped();

	template <typename T>
	void EDITOR_SortByWeightAscendingInternal(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight < B.Weight; });
	}

	/** Sort collection by weights in descending order. */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="⇅ Descending", ShortToolTip="Sort collection by weights in descending order.", DisplayOrder=11))
	void EDITOR_SortByWeightDescending();

	virtual void EDITOR_SortByWeightDescendingTyped();

	template <typename T>
	void EDITOR_SortByWeightDescendingInternal(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight > B.Weight; });
	}

	/**Sort collection by weights in descending order. */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="W = i", ShortToolTip="Set weigth to the entry index.", DisplayOrder=20))
	void EDITOR_SetWeightIndex();

	virtual void EDITOR_SetWeightIndexTyped();

	template <typename T>
	void EDITOR_SetWeightIndexInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = i + 1; }
	}

	/** Add 1 to all weights so it's easier to weight down some assets */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="W += 1", ShortToolTip="Add 1 to all weights so it's easier to weight down some assets", DisplayOrder=21))
	void EDITOR_PadWeight();

	virtual void EDITOR_PadWeightTyped();

	template <typename T>
	void EDITOR_PadWeightInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight += 1; }
	}

	/** Multiplies all weights by 2 */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="W * 2", ShortToolTip="Multiplies weights by 2", DisplayOrder=21))
	void EDITOR_MultWeight2();

	virtual void EDITOR_MultWeight2Typed();

	/** Multiplies all weights by 10 */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="W * 10", ShortToolTip="Multiplies weights by 10", DisplayOrder=22))
	void EDITOR_MultWeight10();

	virtual void EDITOR_MultWeight10Typed();

	template <typename T>
	void EDITOR_MultWeightInternal(TArray<T>& Entries, int32 Mult)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight *= Mult; }
	}

	/** Reset all weights to 100 */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="W = 100", ShortToolTip="Reset all weights to 100", DisplayOrder=23))
	void EDITOR_WeightOne();

	virtual void EDITOR_WeightOneTyped();

	template <typename T>
	void EDITOR_WeightOneInternal(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = 100; }
	}

	/** Assign random weights to items */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="Randomize Weights", ShortToolTip="Assign random weights to items", DisplayOrder=24))
	void EDITOR_WeightRandom();

	virtual void EDITOR_WeightRandomTyped();

	template <typename T>
	void EDITOR_WeightRandomInternal(TArray<T>& Entries)
	{
		FRandomStream RandomSource(FMath::Rand());
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = RandomSource.RandRange(1, Entries.Num() * 100); }
	}

	/** Normalize weight sum to 100 */
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="Normalized Weights Sum", ShortToolTip="Normalize weight sum to 100", DisplayOrder=25))
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

		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);

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

		if (PathIdentity->UnderlyingType == EPCGMetadataTypes::SoftObjectPath)
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FSoftObjectPath, PathIdentity->Identifier, { SetEntryPath(i, V); })
		}
		else if (PathIdentity->UnderlyingType == EPCGMetadataTypes::String)
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FString, PathIdentity->Identifier, { SetEntryPath(i, FSoftObjectPath(V)); })
		}
		else
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FName, PathIdentity->Identifier, { SetEntryPath(i, FSoftObjectPath(V.ToString())); })
		}


		// Weight value
		if (WeightIdentity)
		{
#define PCGEX_ATT_TOINT32(_NAME, _TYPE)\
			if (WeightIdentity->UnderlyingType == EPCGMetadataTypes::_NAME){ \
				PCGEX_FOREACH_COLLECTION_ENTRY(_TYPE, WeightIdentity->Identifier, {  InCollection->Entries[i].Weight = static_cast<int32>(V); }) }

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
				PCGEX_FOREACH_COLLECTION_ENTRY(FString, WeightIdentity->Identifier, { InCollection->Entries[i].Category = FName(V); })
			}
			else if (CategoryIdentity->UnderlyingType == EPCGMetadataTypes::Name)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FName, WeightIdentity->Identifier, { InCollection->Entries[i].Category = V; })
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
struct PCGEXTENDEDTOOLKIT_API FPCGExRoamingAssetCollectionDetails : public FPCGExAssetAttributeSetDetails
{
	GENERATED_BODY()

	FPCGExRoamingAssetCollectionDetails() = default;

	explicit FPCGExRoamingAssetCollectionDetails(const TSubclassOf<UPCGExAssetCollection>& InAssetCollectionType);

	UPROPERTY()
	bool bSupportCustomType = true;

	/** Defines what type of temp collection to build from input attribute set */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportCustomType", EditConditionHides, HideEditConditionToggle))
	TSubclassOf<UPCGExAssetCollection> AssetCollectionType;

	bool Validate(FPCGExContext* InContext) const;

	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const bool bBuildStaging = false) const;

	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const FName InputPin, const bool bBuildStaging) const;
};

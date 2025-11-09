// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExMath.h"
#include "PCGExSettingsMacros.h"
#include "Data/PCGExDataFilter.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExDetailsStaging.generated.h"

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
}


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAssetDistributionIndexDetails
{
	GENERATED_BODY()

	FPCGExAssetDistributionIndexDetails();
	
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Index Settings", EditCondition="Distribution == EPCGExDistribution::Index"))
	FPCGExAssetDistributionIndexDetails IndexSettings;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 LocalSeed = 0;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMicroCacheDistributionDetails
{
	GENERATED_BODY()

	FPCGExMicroCacheDistributionDetails() = default;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Index Settings", EditCondition="Distribution == EPCGExDistribution::Index"))
	FPCGExAssetDistributionIndexDetails IndexSettings;

	/** Note that this is only accounted for if selected in the seed component. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
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


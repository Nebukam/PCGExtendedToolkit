// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCollectionsCommon.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Math/PCGExMath.h"
#include "PCGExStagingDetails.generated.h"

class UPCGExBitmaskCollection;
class UPCGParamData;

USTRUCT(BlueprintType)
struct PCGEXCOLLECTIONS_API FPCGExEntryTypeDetails
{
	GENERATED_BODY()

	FPCGExEntryTypeDetails();

	/** Name of the int64 that will hold entry type as a bitmask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName EntryTypeAttributeName = FName("EntryType");

	/** Bitmask collection containing the flags to apply per entry type. Is expected to have the following bitmasks identifiers:
	 * - Collection
	 * - Mesh
	 * - Actor
	 * - PCGDataAsset
	 * Note that "Collection" will be OR'd to subcollection with their matching specific type; i.e Collection | Mesh for a MeshCollection. 
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExBitmaskCollection> EntryTypes;
};

USTRUCT(BlueprintType)
struct PCGEXCOLLECTIONS_API FPCGExAssetDistributionIndexDetails
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
struct PCGEXCOLLECTIONS_API FPCGExComponentTaggingDetails
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
struct PCGEXCOLLECTIONS_API FPCGExAssetTaggingDetails : public FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExAssetTaggingDetails() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExCollections.EPCGExAssetTagInheritance"))
	uint8 GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Asset);

	bool IsEnabled() const { return GrabTags != 0; }
};

USTRUCT(BlueprintType)
struct PCGEXCOLLECTIONS_API FPCGExAssetDistributionDetails
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExCore.EPCGExSeedComponents"))
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
struct PCGEXCOLLECTIONS_API FPCGExMicroCacheDistributionDetails
{
	GENERATED_BODY()

	FPCGExMicroCacheDistributionDetails() = default;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExCore.EPCGExSeedComponents"))
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
struct PCGEXCOLLECTIONS_API FPCGExAssetAttributeSetDetails
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
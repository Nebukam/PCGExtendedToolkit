// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "PCGExAssetCollectionToSet.generated.h"

class UPCGExAssetCollection;
struct FPCGExAssetCollectionEntry;

UENUM()
enum class EPCGExSubCollectionToSet : uint8
{
	Ignore             = 0 UMETA(DisplayName = "Ignore", Tooltip="Ignore sub-collections"),
	Expand             = 1 UMETA(DisplayName = "Expand", Tooltip="Expand the entire sub-collection"),
	PickRandom         = 2 UMETA(DisplayName = "Random", Tooltip="Pick one at random"),
	PickRandomWeighted = 3 UMETA(DisplayName = "Random weighted", Tooltip="Pick one at random, weighted"),
	PickFirstItem      = 4 UMETA(DisplayName = "First item", Tooltip="Pick the first item"),
	PickLastItem       = 5 UMETA(DisplayName = "Last item", Tooltip="Pick the last item"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="assets-management/asset-collection-to-set"))
class UPCGExAssetCollectionToSetSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExAssetCollectionToSetElement;

	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AssetCollectionToSet, "Asset Collection to Set", "Converts an asset collection to an attribute set.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** The asset collection to convert to an attribute set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	/** Attribute names */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubCollectionToSet SubCollectionHandling = EPCGExSubCollectionToSet::PickRandomWeighted;

	/** If enabled, allows duplicate entries (duplicate is same object path & category) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowDuplicates = true;

	/** If enabled, invalid or empty entries are removed from the output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitInvalidAndEmpty = true;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAssetPath = true;

	UPROPERTY(meta=(PCG_NotOverridable))
	bool bWriteAssetClass = true;

	/** Name of the attribute on the AttributeSet that contains the asset path to be staged */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Asset Path", EditCondition="bWriteAssetPath"))
	FName AssetPathAttributeName = FName("AssetPath");

	UPROPERTY()
	FName AssetClassAttributeName = NAME_None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteWeight = true;

	/** Name of the attribute on the AttributeSet that contains the asset weight, if any. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="bWriteWeight"))
	FName WeightAttributeName = FName("Weight");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCategory = false;

	/** Name of the attribute on the AttributeSet that contains the asset category, if any. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Category", EditCondition="bWriteCategory"))
	FName CategoryAttributeName = FName("Category");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteExtents = false;

	/** Name of the attribute on the AttributeSet that contains the asset bounds' Extents, if any. Otherwise 0 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Extents", EditCondition="bWriteExtents"))
	FName ExtentsAttributeName = FName("Extents");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteBoundsMin = false;

	/** Name of the attribute on the AttributeSet that contains the asset BoundsMin, if any. Otherwise 0 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="BoundsMin", EditCondition="bWriteBoundsMin"))
	FName BoundsMinAttributeName = FName("BoundsMin");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteBoundsMax = false;

	/** Name of the attribute on the AttributeSet that contains the asset BoundsMax, if any. Otherwise 0 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="BoundsMax", EditCondition="bWriteBoundsMax"))
	FName BoundsMaxAttributeName = FName("BoundsMax");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNestingDepth = false;

	/** Name of the attribute on the AttributeSet that contains the asset depth, if any. Otherwise -1 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Nesting Depth", EditCondition="bWriteNestingDepth"))
	FName NestingDepthAttributeName = FName("NestingDepth");
};

class FPCGExAssetCollectionToSetElement final : public IPCGExElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;

protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
	static void ProcessEntry(const FPCGExAssetCollectionEntry* InEntry, TArray<const FPCGExAssetCollectionEntry*>& OutEntries, const bool bOmitInvalidAndEmpty, const bool bNoDuplicates, const EPCGExSubCollectionToSet SubHandling, TSet<uint64>& GUIDS);
};

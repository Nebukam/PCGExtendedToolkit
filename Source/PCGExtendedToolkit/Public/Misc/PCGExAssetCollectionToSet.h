// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "AssetSelectors/PCGExAssetCollection.h"

#include "PCGExAssetCollectionToSet.generated.h"

UENUM(BlueprintType)
enum class EPCGExSubCollectionToSet : uint8
{
	Ignore UMETA(DisplayName = "Ignore", Tooltip="Ignore sub-collections"),
	Expand UMETA(DisplayName = "Expand", Tooltip="Expand the entire sub-collection"),
	PickRandom UMETA(DisplayName = "Random", Tooltip="Pick one at random"),
	PickRandomWeighted UMETA(DisplayName = "Random weighted", Tooltip="Pick one at random, weighted"),
	PickFirstItem UMETA(DisplayName = "First item", Tooltip="Pick the first item"),
	PickLastItem UMETA(DisplayName = "Last item", Tooltip="Pick the last item"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetCollectionToSetSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExAssetCollectionToSetElement;

public:
	bool bCacheResult = false;
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AssetCollectionToSet, "Asset Collection to Set", "Converts an asset collection to an attribute set.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** The asset collection to convert to an attribute set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	/** Output attribute names */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExAssetAttributeSetDetails OutputAttributes;

	/** Attribute names */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubCollectionToSet SubCollectionHandling = EPCGExSubCollectionToSet::PickRandomWeighted;

	/** If enabled, allows duplicate entries (duplicate is same object path & category) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowDuplicates = true;

	/** If enabled, invalid or empty entries are removed from the output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitInvalidAndEmpty = true;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetCollectionToSetElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	//virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	static void ProcessStagingData(
		const FPCGExAssetStagingData* InStagingData,
		TArray<int32>& Weights,
		TArray<FSoftObjectPath>& Paths,
		TArray<FName>& Categories, const bool bOmitInvalidAndEmpty, const EPCGExSubCollectionToSet SubHandling, TSet<uint64>& GUIDS);
};

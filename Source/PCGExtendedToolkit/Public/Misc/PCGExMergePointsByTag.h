// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSortPoints.h"
#include "PCGExMergePointsByTag.generated.h"

class FPCGExPointIOMerger;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Merge by Tag | Reoslution Mode"))
enum class EPCGExMergeByTagOverlapResolutionMode : uint8
{
	Strict           = 0 UMETA(DisplayName = "Strict", ToolTip="Merge happens per-tag, and higher priority tags are removed from lower priority overlaps."),
	ImmediateOverlap = 1 UMETA(DisplayName = "Overlap", ToolTip="Merge happens per-tag, overlapping data is merged entierely."),
	Flatten          = 2 UMETA(DisplayName = "Flatten", ToolTip="Flatten all tags into a unique identifier and match-merge based on that identifier."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Merge by Tag | Fallback Behavior"))
enum class EPCGExMergeByTagFallbackBehavior : uint8
{
	Omit    = 0 UMETA(DisplayName = "Omit", ToolTip="Do not output data that didn't pass filters"),
	Merge   = 1 UMETA(DisplayName = "Merge", ToolTip="Merge all data that didn't pass filter in a single blob"),
	Forward = 2 UMETA(DisplayName = "Forward", ToolTip="Forward data that didn't pass filter without merging them"),
};

namespace PCPGExMergePointsByTag
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FMergeList
	{
	public:
		TArray<PCGExData::FPointIO*> IOs;
		PCGExData::FPointIO* CompositeIO = nullptr;
		FPCGExPointIOMerger* Merger = nullptr;

		FMergeList();
		~FMergeList();

		void Merge(PCGExMT::FTaskManager* AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails);
		void Write(PCGExMT::FTaskManager* AsyncManager) const;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTagBucket
	{
	public:
		FString Tag;
		TArray<PCGExData::FPointIO*> IOs;
		explicit FTagBucket(const FString& InTag);
		~FTagBucket();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTagBuckets
	{
	public:
		TArray<FTagBucket*> Buckets;
		TMap<FString, int32> BucketsMap;
		TMap<PCGExData::FPointIO*, TSet<FTagBucket*>*> ReverseBucketsMap;
		explicit FTagBuckets();
		~FTagBuckets();

		void Distribute(PCGExData::FPointIO* IO, const FPCGExNameFiltersDetails& Filters);
		void AddToReverseMap(PCGExData::FPointIO* IO, FTagBucket* Bucket);
		void BuildMergeLists(EPCGExMergeByTagOverlapResolutionMode Mode, TArray<FMergeList*>& OutLists, const TArray<FString>& Priorities, const EPCGExSortDirection SortDirection);
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMergePointsByTagSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergePointsByTag, "Merge Points by Tag", "Merge points based on shared tags.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMergeByTagOverlapResolutionMode Mode = EPCGExMergeByTagOverlapResolutionMode::Strict;

	/** Sorting direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode!=EPCGExMergeByTagOverlapResolutionMode::Flatten", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Descending;

	/** Fallback behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExMergeByTagOverlapResolutionMode::Flatten", EditConditionHides))
	EPCGExMergeByTagFallbackBehavior FallbackBehavior = EPCGExMergeByTagFallbackBehavior::Omit;
	
	/** Tags to be processed or ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNameFiltersDetails TagFilters;

	/** Which tag has merging authority over another. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TArray<FString> ResolutionPriorities;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergePointsByTagContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExMergePointsByTagElement;

	virtual ~FPCGExMergePointsByTagContext() override;

	FPCGExNameFiltersDetails TagFilters;
	FPCGExCarryOverDetails CarryOverDetails;

	PCPGExMergePointsByTag::FMergeList* FallbackMergeList = nullptr;
	TMap<uint32, PCPGExMergePointsByTag::FMergeList*> MergeMap;
	TArray<PCPGExMergePointsByTag::FMergeList*> MergeLists;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergePointsByTagElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExMergePointsByTag
{
}

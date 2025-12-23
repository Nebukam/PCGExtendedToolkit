// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Sorting/PCGExSortingCommon.h"
#include "Utils/PCGExPointIOMerger.h"

#include "PCGExMergePointsByTag.generated.h"

UENUM()
enum class EPCGExMergeByTagOverlapResolutionMode : uint8
{
	Strict           = 0 UMETA(DisplayName = "Strict", ToolTip="Merge happens per-tag, and higher priority tags are removed from lower priority overlaps."),
	ImmediateOverlap = 1 UMETA(DisplayName = "Overlap", ToolTip="Merge happens per-tag, overlapping data is merged entirely."),
	Flatten          = 2 UMETA(DisplayName = "Flatten", ToolTip="Flatten all tags into a unique identifier and match-merge based on that identifier."),
};

UENUM()
enum class EPCGExMergeByTagFallbackBehavior : uint8
{
	Omit    = 0 UMETA(DisplayName = "Omit", ToolTip="Do not output data that didn't pass filters"),
	Merge   = 1 UMETA(DisplayName = "Merge", ToolTip="Merge all data that didn't pass filter in a single blob"),
	Forward = 2 UMETA(DisplayName = "Forward", ToolTip="Forward data that didn't pass filter without merging them"),
};

namespace PCPGExMergePointsByTag
{
	PCGEX_CTX_STATE(State_MergingData);

	class FMergeList
	{
	public:
		TArray<TSharedPtr<PCGExData::FPointIO>> IOs;
		TSharedPtr<PCGExData::FFacade> CompositeDataFacade;
		TSharedPtr<FPCGExPointIOMerger> Merger;

		FMergeList();
		~FMergeList() = default;

		void Merge(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const FPCGExCarryOverDetails* InCarryOverDetails);
		void Write(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) const;
	};

	class FTagBucket
	{
	public:
		FString Tag;
		TArray<TSharedPtr<PCGExData::FPointIO>> IOs;
		explicit FTagBucket(const FString& InTag);
		~FTagBucket() = default;
	};

	class FTagBuckets
	{
	public:
		TArray<TSharedPtr<FTagBucket>> Buckets;
		TMap<FString, int32> BucketsMap;
		TMap<PCGExData::FPointIO*, TSharedPtr<TSet<TSharedPtr<FTagBucket>>>> ReverseBucketsMap;
		explicit FTagBuckets();
		~FTagBuckets() = default;

		void Distribute(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& IO, const FPCGExNameFiltersDetails& Filters);
		void AddToReverseMap(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<FTagBucket>& Bucket);
		void BuildMergeLists(FPCGExContext* InContext, EPCGExMergeByTagOverlapResolutionMode Mode, TArray<TSharedPtr<FMergeList>>& OutLists, const TArray<FString>& Priorities, const EPCGExSortDirection SortDirection);
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/merge-points-by-tag"))
class UPCGExMergePointsByTagSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergePointsByTag, "Merge Points by Tag", "Merge points based on shared tags.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMergeByTagOverlapResolutionMode Mode = EPCGExMergeByTagOverlapResolutionMode::Strict;

	/** Sorting direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExMergeByTagOverlapResolutionMode::Flatten", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Descending;

	/** Fallback behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExMergeByTagOverlapResolutionMode::Flatten", EditConditionHides))
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

struct FPCGExMergePointsByTagContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMergePointsByTagElement;

	FPCGExNameFiltersDetails TagFilters;
	FPCGExCarryOverDetails CarryOverDetails;

	TSharedPtr<PCPGExMergePointsByTag::FMergeList> FallbackMergeList;
	TMap<uint32, TSharedPtr<PCPGExMergePointsByTag::FMergeList>> MergeMap;
	TArray<TSharedPtr<PCPGExMergePointsByTag::FMergeList>> MergeLists;
};

class FPCGExMergePointsByTagElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(MergePointsByTag)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExMergePointsByTag
{
}

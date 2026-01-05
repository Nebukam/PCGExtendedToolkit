// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clipper2Lib/clipper.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExMatchingDetails.h"
#include "Math/PCGExProjectionDetails.h"

#include "PCGExClipper2Processor.generated.h"

struct FPCGExBlendingDetails;
struct FPCGExCarryOverDetails;


UENUM(BlueprintType)
enum class EPCGExClipper2JoinType : uint8
{
	Square = 0 UMETA(DisplayName = "Square", ToolTip="Square joins"),
	Round  = 1 UMETA(DisplayName = "Round", ToolTip="Round joins"),
	Bevel  = 2 UMETA(DisplayName = "Bevel", ToolTip="Bevel joins"),
	Miter  = 3 UMETA(DisplayName = "Miter", ToolTip="Miter joins"),
};

UENUM(BlueprintType)
enum class EPCGExClipper2EndType : uint8
{
	Polygon = 0 UMETA(DisplayName = "Polygon", ToolTip="Closed polygon (ignores path open/closed state)"),
	Joined  = 1 UMETA(DisplayName = "Joined", ToolTip="Joined ends"),
	Butt    = 2 UMETA(DisplayName = "Butt", ToolTip="Butt ends"),
	Square  = 3 UMETA(DisplayName = "Square", ToolTip="Square ends"),
	Round   = 4 UMETA(DisplayName = "Round", ToolTip="Round ends"),
};

namespace PCGExClipper2
{
	
	// Special marker for intersection points - uses high bit pattern that's unlikely in normal usage
	constexpr uint32 INTERSECTION_MARKER = 0xFFFFFFFF;

	enum class EMainGroupingPolicy : uint8
	{
		Split,
		Consolidate
	};
	
	// Intersection blend info - stores the 4 contributing source points
	struct FIntersectionBlendInfo
	{
		// Edge 1: bot -> top
		uint32 E1BotPointIdx = 0;
		uint32 E1BotSourceIdx = 0;
		uint32 E1TopPointIdx = 0;
		uint32 E1TopSourceIdx = 0;

		// Edge 2: bot -> top
		uint32 E2BotPointIdx = 0;
		uint32 E2BotSourceIdx = 0;
		uint32 E2TopPointIdx = 0;
		uint32 E2TopSourceIdx = 0;

		// Alpha along each edge (0-1)
		double E1Alpha = 0.5;
		double E2Alpha = 0.5;
	};

	class FOpData : public TSharedFromThis<FOpData>
	{
	public:
		TArray<TSharedPtr<PCGExData::FFacade>> Facades;
		TArray<PCGExClipper2Lib::Path64> Paths;
		TArray<bool> IsClosedLoop;
		TArray<FPCGExGeo2DProjectionDetails> Projections;

		// Map from Facade->Idx to array index for quick lookup
		TMap<int32, int32> FacadeIdxToArrayIndex;

		explicit FOpData(const int32 InReserve);
		void AddReserve(const int32 InReserve);

		int32 Num() const { return Facades.Num(); }

		// Find index in Facades array by Facade->Idx (uses map for O(1) lookup)
		FORCEINLINE int32 FindSourceIndex(const int32 FacadeIdx) const
		{
			const int32* Found = FacadeIdxToArrayIndex.Find(FacadeIdx);
			return Found ? *Found : INDEX_NONE;
		}

		// Register a facade in the lookup
		void RegisterFacade(const TSharedPtr<PCGExData::FFacade>& Facade, int32 ArrayIndex);
	};

	/**
	 * Unified processing group that encapsulates subjects, operands, and cached data.
	 * This provides a clean interface for Clipper2 operations.
	 */
	struct FProcessingGroup : public TSharedFromThis<FProcessingGroup>
	{
		// Indices into AllOpData
		TArray<int32> SubjectIndices;
		TArray<int32> OperandIndices;

		// Cached paths for quick access (references into AllOpData->Paths)
		PCGExClipper2Lib::Paths64 SubjectPaths;
		PCGExClipper2Lib::Paths64 OperandPaths;

		// Combined source indices for blending
		TArray<int32> AllSourceIndices;

		// Intersection blend info map: keyed by encoded (x,y) position
		TMap<uint64, FIntersectionBlendInfo> IntersectionBlendInfos;
		mutable FCriticalSection IntersectionLock;

		FProcessingGroup() = default;

		// Prepare cached paths from AllOpData
		void Prepare(const TSharedPtr<FOpData>& AllOpData);

		// Check if this group is valid for processing
		bool IsValid() const { return !SubjectPaths.empty(); }

		// Add intersection blend info (thread-safe)
		void AddIntersectionBlendInfo(int64_t X, int64_t Y, const FIntersectionBlendInfo& Info);

		// Get intersection blend info by position
		const FIntersectionBlendInfo* GetIntersectionBlendInfo(int64_t X, int64_t Y) const;

		// Create the ZCallback for this group
		PCGExClipper2Lib::ZCallback64 CreateZCallback();
	};
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Cavalier")
class UPCGExClipper2ProcessorSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExClipper2ProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Path); }
#endif
	//~End UPCGSettings

protected:
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	/** If enabled, lets you to create sub-groups to operate on. If disabled, data is processed individually. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails MainDataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);

	/** If enabled, lets you to pick which are matched with which main data.
	 * Note that the match is done against every single data within a group and then consolidated;
	 * this means if you have a [A,B,C] group, ABC will be executed against operands for A, B and C individually. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="NeedsOperands()", EditConditionHides))
	FPCGExMatchingDetails OperandsDataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);

	/** Skip paths that aren't closed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSkipOpenPaths = false;

	/** Decimal precision 
	 * Clipper2 Uses int64 under the hood to preserve extreme precision, so we scale floating point values then back. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1), AdvancedDisplay)
	int32 Precision = 10;

	/** Cleanup */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSimplifyPaths = true;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagHoles = false;

	/** Write this tag on paths that are holes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagHoles"))
	FString HoleTag = TEXT("Hole");

	UFUNCTION()
	virtual bool NeedsOperands() const;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const;
	virtual PCGExClipper2::EMainGroupingPolicy GetGroupingPolicy() const;
};

struct FPCGExClipper2ProcessorContext : FPCGExPathProcessorContext
{
	friend class FPCGExClipper2ProcessorElement;

	std::atomic<int32> NextSourceId = 0;

	int32 AllocateSourceIdx() { return NextSourceId.fetch_add(1); }

	TSharedPtr<PCGExData::FPointIOCollection> OperandsCollection;

	TSharedPtr<PCGExClipper2::FOpData> AllOpData;

	// Unified processing groups
	TArray<TSharedPtr<PCGExClipper2::FProcessingGroup>> ProcessingGroups;

	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/**
	 * Convert Clipper2 Paths64 results back to PCGEx point data with metadata blending.
	 * Uses original transforms from source points and handles intersection blending.
	 * 
	 * @param InPaths - The Clipper2 output paths to convert
	 * @param Group - The processing group containing source info and intersection blend data
	 * @param InBlendingDetails - Optional blending configuration for metadata
	 * @param InCarryOverDetails - Optional carry-over configuration
	 * @param OutPaths - Output array of point IO objects
	 */
	void OutputPaths64(
		PCGExClipper2Lib::Paths64& InPaths,
		const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group,
		const FPCGExBlendingDetails* InBlendingDetails,
		const FPCGExCarryOverDetails* InCarryOverDetails,
		TArray<TSharedPtr<PCGExData::FPointIO>>& OutPaths);

	/**
	 * Process a single group. Derived classes must implement this.
	 * @param Group - The processing group to operate on
	 */
	virtual void Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group);
};

class FPCGExClipper2ProcessorElement : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Processor)
	
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	int32 BuildDataFromCollection(
		FPCGExClipper2ProcessorContext* Context,
		const UPCGExClipper2ProcessorSettings* Settings,
		const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
		TArray<int32>& OutIndices) const;

	void BuildProcessingGroups(
		FPCGExClipper2ProcessorContext* Context,
		const UPCGExClipper2ProcessorSettings* Settings,
		const TArray<int32>& MainIndices,
		const TArray<int32>& OperandIndices) const;
};

namespace PCGExClipper2
{
}
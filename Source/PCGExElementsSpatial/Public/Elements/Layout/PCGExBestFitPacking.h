// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExLayout.h"
#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExSettingsMacros.h"
#include "Sorting/PCGExSortingCommon.h"
#include "Math/PCGExUVW.h"

#include "PCGExBestFitPacking.generated.h"

struct FPCGExSortRuleConfig;

namespace PCGExSorting
{
	class FSorter;
}

namespace PCGExData
{
	struct FMutablePoint;
}

UENUM()
enum class EPCGExBestFitRotationMode : uint8
{
	None            = 0 UMETA(DisplayName = "None", ToolTip="No rotation testing"),
	CardinalOnly    = 1 UMETA(DisplayName = "Cardinal (90°)", ToolTip="Test 90° rotations only (4 orientations)"),
	AllOrthogonal   = 2 UMETA(DisplayName = "All Orthogonal", ToolTip="Test all 24 orthogonal orientations"),
};

UENUM()
enum class EPCGExBestFitScoreMode : uint8
{
	TightestFit     = 0 UMETA(DisplayName = "Tightest Fit", ToolTip="Prioritize spaces where the item fits most tightly"),
	SmallestSpace   = 1 UMETA(DisplayName = "Smallest Space", ToolTip="Prioritize smallest space that can contain the item"),
	LeastWaste      = 2 UMETA(DisplayName = "Least Waste", ToolTip="Minimize volume wasted after placement"),
	Balanced        = 3 UMETA(DisplayName = "Balanced", ToolTip="Balance between tight fit and space conservation"),
};

UENUM()
enum class EPCGExBestFitPlacementAnchor : uint8
{
	Corner          = 0 UMETA(DisplayName = "Corner", ToolTip="Place items at corner of free space"),
	Center          = 1 UMETA(DisplayName = "Center", ToolTip="Place items at center of free space"),
	SeedProximity   = 2 UMETA(DisplayName = "Seed Proximity", ToolTip="Place items as close to seed as possible"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/layout/best-fit-packing"))
class UPCGExBestFitPackingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BestFitPacking, "Best Fit Packing", "Optimal bin packing using best-fit decreasing algorithm with rotation support.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Transform); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
	/** Controls the order in which points will be sorted, when using sorting rules. Uses largest-first by default for best-fit decreasing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Descending;

	/** If enabled, items will be sorted by volume (largest first) before packing. This is the classic BFD approach. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSortByVolume = true;

	/** Per-bin seed. Represent a bound-relative location to start packing from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBinSeedMode SeedMode = EPCGExBinSeedMode::UVWConstant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode == EPCGExBinSeedMode::UVWConstant", EditConditionHides))
	FVector SeedUVW = FVector(-1, -1, -1);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode == EPCGExBinSeedMode::UVWAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SeedUVWAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode == EPCGExBinSeedMode::PositionConstant", EditConditionHides))
	FVector SeedPosition = FVector(0, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode == EPCGExBinSeedMode::PositionAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SeedPositionAttribute;

	/** Scoring method for selecting the best placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExBestFitScoreMode ScoreMode = EPCGExBestFitScoreMode::TightestFit;

	/** Rotation testing mode. More rotations = better fit but slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExBestFitRotationMode RotationMode = EPCGExBestFitRotationMode::CardinalOnly;

	/** How to position items within their chosen space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExBestFitPlacementAnchor PlacementAnchor = EPCGExBestFitPlacementAnchor::Corner;

	/** The main split axis for creating new free spaces after placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExAxis SplitAxis = EPCGExAxis::Up;

	/** Space splitting mode after item placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExSpaceSplitMode SplitMode = EPCGExSpaceSplitMode::Minimal;

	/** If enabled, fitting will try to avoid wasted space by not creating free spaces below a threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bAvoidWastedSpace = true;

	/** Minimum space threshold as a ratio of the smallest item dimension. Spaces smaller than this are discarded. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, EditCondition="bAvoidWastedSpace", ClampMin=0.1, ClampMax=1.0))
	double WastedSpaceThreshold = 0.5;

	/** If enabled, will evaluate all bins for each item to find the globally best placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bGlobalBestFit = true;

	/** Weight for tightness in balanced scoring mode. Higher = prefer tighter fits. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, EditCondition="ScoreMode == EPCGExBestFitScoreMode::Balanced", ClampMin=0.0, ClampMax=1.0))
	double TightnessWeight = 0.6;

	/** Occupation padding source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExInputValueType OccupationPaddingInput = EPCGExInputValueType::Constant;

	/** Occupation padding attribute -- Will be broadcast to FVector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, DisplayName="Occupation Padding (Attr)", EditCondition="OccupationPaddingInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OccupationPaddingAttribute;

	/** Occupation padding constant value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, DisplayName="Occupation Padding", EditCondition="OccupationPaddingInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector OccupationPadding = FVector::ZeroVector;

	/** If enabled, the padding will not be relative (rotated) if the item is rotated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bAbsolutePadding = true;

	/** If enabled, won't throw a warning if there are more bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTooManyBinsWarning = false;

	/** If enabled, won't throw a warning if there are fewer bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTooFewBinsWarning = false;

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;

	PCGEX_SETTING_VALUE_DECL(Padding, FVector)
};

struct FPCGExBestFitPackingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBestFitPackingElement;

	TSet<int32> ValidIOIndices;

	TSharedPtr<PCGExData::FPointIOCollection> Bins;
	TArray<FPCGExUVW> BinsUVW;

	TSharedPtr<PCGExData::FPointIOCollection> Discarded;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBestFitPackingElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BestFitPacking)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBestFitPacking
{
	using PCGExLayout::FSpace;

	// Extended item struct that includes rotation information
	struct FBestFitItem
	{
		int32 Index = -1;
		FBox Box = FBox(EForceInit::ForceInit);
		FVector Padding = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector OriginalSize = FVector::ZeroVector; // Size before rotation

		FBestFitItem() = default;
	};

	// Represents a potential placement with all relevant scoring data
	struct FPlacementCandidate
	{
		int32 BinIndex = -1;
		int32 SpaceIndex = -1;
		int32 RotationIndex = 0;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector RotatedSize = FVector::ZeroVector;
		double Score = MAX_dbl;
		double TightnessScore = 0.0;
		double WasteScore = 0.0;
		double ProximityScore = 0.0;

		bool IsValid() const { return BinIndex >= 0 && SpaceIndex >= 0; }
	};

	class FBinSplit : public TSharedFromThis<FBinSplit>
	{
	public:
		FBinSplit() = default;
		virtual ~FBinSplit() = default;

		virtual void SplitSpace(const FSpace& Space, FBox& ItemBox, TArray<FBox>& OutPartitions) const = 0;
	};

	template <EPCGExAxis SplitAxis = EPCGExAxis::Up, EPCGExSpaceSplitMode Mode = EPCGExSpaceSplitMode::Minimal>
	class TBinSplit : public FBinSplit
	{
	public:
		virtual void SplitSpace(const FSpace& Space, FBox& ItemBox, TArray<FBox>& OutPartitions) const override
		{
			PCGExLayout::SplitSpace<SplitAxis, Mode>(Space, ItemBox, OutPartitions);
		}
	};

	class FBestFitBin : public TSharedFromThis<FBestFitBin>
	{
	protected:
		double MaxVolume = 0;
		double MaxDist = 0;
		double UsedVolume = 0;
		double MinOccupation = 0;
		FVector Seed = FVector::ZeroVector;
		TSharedPtr<FBinSplit> Splitter;

		TArray<FSpace> Spaces;
		void AddSpace(const FBox& InBox);
		void RemoveSmallSpaces(double MinSize);

	public:
		int32 BinIndex = -1;
		int32 MaxItems = 0;
		FBox Bounds;
		FTransform Transform;

		const UPCGExBestFitPackingSettings* Settings = nullptr;
		FVector WastedSpaceThresholds = FVector::ZeroVector;
		TArray<FBestFitItem> Items;

		FBestFitBin(int32 InBinIndex, const PCGExData::FConstPoint& InBinPoint, const FVector& InSeed, const TSharedPtr<FBinSplit>& InSplitter);
		~FBestFitBin() = default;

		void SetMinOccupation(double InMinOccupation) { MinOccupation = InMinOccupation; }

		bool IsFull() const { return Items.Num() >= MaxItems; }
		double GetFillRatio() const { return MaxVolume > 0 ? UsedVolume / MaxVolume : 0; }
		double GetRemainingVolume() const { return MaxVolume - UsedVolume; }
		int32 GetSpaceCount() const { return Spaces.Num(); }
		const FSpace& GetSpace(int32 Index) const { return Spaces[Index]; }
		const FVector& GetSeed() const { return Seed; }
		double GetMaxVolume() const { return MaxVolume; }
		double GetMaxDist() const { return MaxDist; }

		// Evaluate how well an item fits in a specific space
		bool EvaluatePlacement(
			const FVector& ItemSize,
			int32 SpaceIndex,
			const FRotator& Rotation,
			FPlacementCandidate& OutCandidate) const;

		// Commit a placement
		void CommitPlacement(const FPlacementCandidate& Candidate, FBestFitItem& InItem);

		void UpdatePoint(PCGExData::FMutablePoint& InPoint, const FBestFitItem& InItem) const;
	};

	// Rotation utilities
	class FRotationHelper
	{
	public:
		static void GetRotationsToTest(EPCGExBestFitRotationMode Mode, TArray<FRotator>& OutRotations);
		static FVector RotateSize(const FVector& Size, const FRotator& Rotation);
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBestFitPackingContext, UPCGExBestFitPackingSettings>
	{
	protected:
		TSharedPtr<FBinSplit> Splitter;
		double MinOccupation = 0;
		TSharedPtr<PCGExSorting::FSorter> Sorter;
		TArray<TSharedPtr<FBestFitBin>> Bins;
		TBitArray<> Fitted;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> PaddingBuffer;
		bool bHasUnfitted = false;

		TArray<int32> ProcessingOrder;
		TArray<FRotator> RotationsToTest;

		// Find the globally best placement across all bins (or sequentially if not global)
		FPlacementCandidate FindBestPlacement(const FBestFitItem& InItem);

		// Compute the final score based on settings
		double ComputeFinalScore(const FPlacementCandidate& Candidate) const;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
			bForceSingleThreadedProcessPoints = true;
		}

		virtual ~FProcessor() override
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
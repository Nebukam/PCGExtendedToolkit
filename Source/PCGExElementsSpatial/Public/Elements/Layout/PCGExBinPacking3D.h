// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExLayout.h"
#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExSettingsMacros.h"
#include "Sorting/PCGExSortingCommon.h"
#include "Math/PCGExUVW.h"

#include "PCGExBinPacking3D.generated.h"

struct FPCGExSortRuleConfig;

namespace PCGExSorting
{
	class FSorter;
}

namespace PCGExData
{
	struct FMutablePoint;
}

///
/// Enums
///

UENUM()
enum class EPCGExBP3DRotationMode : uint8
{
	None          = 0 UMETA(DisplayName = "None", ToolTip="No rotation testing"),
	CardinalOnly  = 1 UMETA(DisplayName = "Cardinal (90deg)", ToolTip="Test 90 degree rotations only (4 orientations around up axis)"),
	Paper6        = 2 UMETA(DisplayName = "Paper 6 Orientations", ToolTip="6 axis permutations with symmetry reduction (Q4RealBPP). Cube=1, square prism=3, all-different=6"),
	AllOrthogonal = 3 UMETA(DisplayName = "All Orthogonal", ToolTip="Test all 24 orthogonal orientations"),
};

UENUM()
enum class EPCGExBP3DAffinityType : uint8
{
	Negative = 0 UMETA(DisplayName = "Negative (Incompatible)", ToolTip="Categories must NOT share the same bin"),
	Positive = 1 UMETA(DisplayName = "Positive (Co-locate)", ToolTip="Categories SHOULD share the same bin"),
};

///
/// Affinity Rule Struct
///

USTRUCT(BlueprintType)
struct FPCGExBP3DAffinityRule
{
	GENERATED_BODY()

	FPCGExBP3DAffinityRule() = default;

	/** Type of affinity constraint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBP3DAffinityType Type = EPCGExBP3DAffinityType::Negative;

	/** First category value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 CategoryA = 0;

	/** Second category value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 CategoryB = 1;
};

///
/// Settings
///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/layout/bin-packing-3d"))
class UPCGExBinPacking3DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BinPacking3D, "Bin Packing 3D (Q4RealBPP)", "3D bin packing with real-world constraints from the Q4RealBPP paper: weight limits, category affinities, load bearing, and multi-objective scoring.");
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
	//
	// Settings
	//

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

	//
	// Settings|Fitting
	//

	/** Rotation testing mode. More rotations = better fit but slower. Paper6 uses the paper's symmetry-reduced orientations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExBP3DRotationMode RotationMode = EPCGExBP3DRotationMode::Paper6;

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

	//
	// Settings|Objectives
	//

	/** Weight for bin-usage objective (o1). Higher fill-ratio bins are preferred. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Objectives", meta = (PCG_Overridable, ClampMin=0.0, ClampMax=1.0))
	double ObjectiveWeightBinUsage = 0.5;

	/** Weight for height objective (o2). Lower placement Z is preferred (floor-up packing). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Objectives", meta = (PCG_Overridable, ClampMin=0.0, ClampMax=1.0))
	double ObjectiveWeightHeight = 0.3;

	/** Weight for load-balance objective (o3). Placement closer to bin center-of-mass is preferred. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Objectives", meta = (PCG_Overridable, ClampMin=0.0, ClampMax=1.0))
	double ObjectiveWeightLoadBalance = 0.2;

	//
	// Settings|Weight Constraint
	//

	/** Enable maximum weight per bin constraint (Paper Eq. 23). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable))
	bool bEnableWeightConstraint = false;

	/** Source for item weight values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, EditCondition="bEnableWeightConstraint", EditConditionHides))
	EPCGExInputValueType ItemWeightInput = EPCGExInputValueType::Constant;

	/** Attribute to read per-item weight from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, DisplayName="Item Weight (Attr)", EditCondition="bEnableWeightConstraint && ItemWeightInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ItemWeightAttribute;

	/** Constant item weight value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, DisplayName="Item Weight", EditCondition="bEnableWeightConstraint && ItemWeightInput == EPCGExInputValueType::Constant", EditConditionHides))
	double ItemWeight = 1.0;

	/** Source for bin max weight values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, EditCondition="bEnableWeightConstraint", EditConditionHides))
	EPCGExInputValueType BinMaxWeightInput = EPCGExInputValueType::Constant;

	/** Attribute to read per-bin maximum weight from (on bin points). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, DisplayName="Bin Max Weight (Attr)", EditCondition="bEnableWeightConstraint && BinMaxWeightInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector BinMaxWeightAttribute;

	/** Constant maximum bin weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weight Constraint", meta = (PCG_Overridable, DisplayName="Bin Max Weight", EditCondition="bEnableWeightConstraint && BinMaxWeightInput == EPCGExInputValueType::Constant", EditConditionHides))
	double BinMaxWeight = 100.0;

	//
	// Settings|Category Affinities
	//

	/** Enable category-based affinity constraints (Paper Eq. 24-25). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category Affinities", meta = (PCG_Overridable))
	bool bEnableAffinities = false;

	/** Attribute name for per-item integer category. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category Affinities", meta = (PCG_Overridable, EditCondition="bEnableAffinities", EditConditionHides))
	FPCGAttributePropertyInputSelector CategoryAttribute;

	/** List of affinity rules between category pairs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Category Affinities", meta = (PCG_Overridable, EditCondition="bEnableAffinities", EditConditionHides, TitleProperty="Type: {CategoryA} <-> {CategoryB}"))
	TArray<FPCGExBP3DAffinityRule> AffinityRules;

	//
	// Settings|Load Bearing
	//

	/** Enable load-bearing constraint (Paper Eq. 26). Items on top must be lighter than items below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Load Bearing", meta = (PCG_Overridable))
	bool bEnableLoadBearing = false;

	/** Mass ratio threshold (eta). new_weight must be <= eta * supporting_weight. 1.0 = equal weight, 0.5 = top must be half the weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Load Bearing", meta = (PCG_Overridable, EditCondition="bEnableLoadBearing", EditConditionHides, ClampMin=0.01, ClampMax=10.0))
	double LoadBearingThreshold = 1.0;

	//
	// Warnings and Errors
	//

	/** If enabled, won't throw a warning if there are more bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTooManyBinsWarning = false;

	/** If enabled, won't throw a warning if there are fewer bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTooFewBinsWarning = false;

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;

	PCGEX_SETTING_VALUE_DECL(Padding, FVector)
	PCGEX_SETTING_VALUE_DECL(ItemWeight, double)
};

///
/// Context
///

struct FPCGExBinPacking3DContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBinPacking3DElement;

	TSet<int32> ValidIOIndices;

	TSharedPtr<PCGExData::FPointIOCollection> Bins;
	TArray<FPCGExUVW> BinsUVW;

	TSharedPtr<PCGExData::FPointIOCollection> Discarded;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

///
/// Element
///

class FPCGExBinPacking3DElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BinPacking3D)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

///
/// Namespace
///

namespace PCGExBinPacking3D
{
	using PCGExLayout::FSpace;

	// Extended item with weight and category
	struct FBP3DItem
	{
		int32 Index = -1;
		FBox Box = FBox(ForceInit);
		FVector Padding = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector OriginalSize = FVector::ZeroVector;
		double Weight = 0.0;
		int32 Category = -1;

		FBP3DItem() = default;
	};

	// Placement candidate with paper's 3 objective scores
	struct FBP3DPlacementCandidate
	{
		int32 BinIndex = -1;
		int32 SpaceIndex = -1;
		int32 RotationIndex = 0;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector RotatedSize = FVector::ZeroVector;
		double Score = MAX_dbl;

		// Geometric scores
		double TightnessScore = 0.0;
		double WasteScore = 0.0;
		double ProximityScore = 0.0;

		// Paper objective scores
		double BinUsageScore = 0.0;
		double HeightScore = 0.0;
		double LoadBalanceScore = 0.0;

		bool IsValid() const { return BinIndex >= 0 && SpaceIndex >= 0; }
	};

	// Paper's 6 orientations with symmetry reduction
	class FBP3DRotationHelper
	{
	public:
		// Generate the 6 axis-permutation orientations from the paper (Table 2),
		// reduced by symmetry based on the item's actual dimensions.
		static void GetPaper6Rotations(const FVector& ItemSize, TArray<FRotator>& OutRotations);

		// Standard rotation generation (None, Cardinal, AllOrthogonal)
		static void GetRotationsToTest(EPCGExBP3DRotationMode Mode, TArray<FRotator>& OutRotations);

		static FVector RotateSize(const FVector& Size, const FRotator& Rotation);
	};

	// Bin split (reuse same pattern)
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

	// Bin with weight tracking, affinity, and load bearing
	class FBP3DBin : public TSharedFromThis<FBP3DBin>
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

		const UPCGExBinPacking3DSettings* Settings = nullptr;
		FVector WastedSpaceThresholds = FVector::ZeroVector;
		TArray<FBP3DItem> Items;

		// Weight constraint
		double CurrentWeight = 0.0;
		double MaxWeight = 0.0;

		// Affinity: set of categories present in this bin
		TSet<int32> PresentCategories;

		FBP3DBin(int32 InBinIndex, const PCGExData::FConstPoint& InBinPoint, const FVector& InSeed, const TSharedPtr<FBinSplit>& InSplitter);
		~FBP3DBin() = default;

		void SetMinOccupation(double InMinOccupation) { MinOccupation = InMinOccupation; }

		bool IsFull() const { return Items.Num() >= MaxItems; }
		double GetFillRatio() const { return MaxVolume > 0 ? UsedVolume / MaxVolume : 0; }
		double GetRemainingVolume() const { return MaxVolume - UsedVolume; }
		int32 GetSpaceCount() const { return Spaces.Num(); }
		const FSpace& GetSpace(int32 Index) const { return Spaces[Index]; }
		const FVector& GetSeed() const { return Seed; }
		double GetMaxVolume() const { return MaxVolume; }
		double GetMaxDist() const { return MaxDist; }
		FVector GetBinCenter() const { return Bounds.GetCenter(); }

		// Evaluate placement (geometric only)
		bool EvaluatePlacement(
			const FVector& ItemSize,
			int32 SpaceIndex,
			const FRotator& Rotation,
			FBP3DPlacementCandidate& OutCandidate) const;

		// Check load-bearing constraint for a candidate placement
		bool CheckLoadBearing(const FBP3DPlacementCandidate& Candidate, double ItemWeight, double Threshold) const;

		// Commit a placement
		void CommitPlacement(const FBP3DPlacementCandidate& Candidate, FBP3DItem& InItem);

		void UpdatePoint(PCGExData::FMutablePoint& InPoint, const FBP3DItem& InItem) const;
	};

	// Main processor
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBinPacking3DContext, UPCGExBinPacking3DSettings>
	{
	protected:
		TSharedPtr<FBinSplit> Splitter;
		double MinOccupation = 0;
		TSharedPtr<PCGExSorting::FSorter> Sorter;
		TArray<TSharedPtr<FBP3DBin>> Bins;
		TBitArray<> Fitted;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> PaddingBuffer;
		TSharedPtr<PCGExDetails::TSettingValue<double>> ItemWeightBuffer;
		bool bHasUnfitted = false;

		TArray<int32> ProcessingOrder;

		// Affinity lookup structures
		TSet<uint64> NegativeAffinityPairs;
		TMap<int32, int32> PositiveAffinityGroup; // category -> group ID (union-find)

		// Per-item data
		TArray<double> ItemWeights;
		TArray<int32> ItemCategories;

		// Per-bin max weight
		TArray<double> BinMaxWeights;

		// Find the globally best placement across all bins
		FBP3DPlacementCandidate FindBestPlacement(const FBP3DItem& InItem);

		// Compute the final score (geometric + paper objectives)
		double ComputeFinalScore(const FBP3DPlacementCandidate& Candidate) const;

		// Affinity helpers
		void BuildAffinityLookups();
		static uint64 MakeAffinityKey(int32 A, int32 B);
		bool HasNegativeAffinity(int32 CatA, int32 CatB) const;
		int32 FindPositiveGroup(int32 Category) const;

		// Check if item category is compatible with bin
		bool IsCategoryCompatibleWithBin(int32 ItemCategory, const FBP3DBin& Bin) const;
		int32 FindRequiredBinForPositiveAffinity(int32 ItemCategory) const;

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

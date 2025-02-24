// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExLayout.h"

#include "PCGExPointsProcessor.h"


#include "Transform/PCGExTransform.h"


#include "PCGExBinPacking.generated.h"

UENUM()
enum class EPCGExBinFreeSpacePartitionMode : uint8
{
	UVWConstant       = 0 UMETA(DisplayName = "UVW (Constant)", ToolTip="A constant bound-relative position"),
	UVWAttribute      = 1 UMETA(DisplayName = "UVW", ToolTip="A per-bin bound-relative position"),
	PositionConstant  = 2 UMETA(DisplayName = "Position (Constant)", ToolTip="A constant world position"),
	PositionAttribute = 3 UMETA(DisplayName = "Position (Attribute)", ToolTip="A per-bin world position"),
};

UENUM()
enum class EPCGExPlacementFavor : uint8
{
	SeedProximity = 0 UMETA(DisplayName = "Seed Proximity", ToolTip="Favor seed proximity over space conservation"),
	Space         = 1 UMETA(DisplayName = "Space Conservation", ToolTip="Favor space conservation over seed proximity"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class UPCGExBinPackingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BinPacking, "Bin Packing", "[EXPERIMENTAL] An simple bin packing node.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorTransform); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Controls the order in which points will be sorted, when using sorting rules. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Per-bin seed. Represent a bound-relative location to start packing from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBinSeedMode SeedMode = EPCGExBinSeedMode::UVWConstant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode==EPCGExBinSeedMode::UVWConstant", EditConditionHides))
	FVector SeedUVW = FVector(0, 0, -1);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode==EPCGExBinSeedMode::UVWAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SeedUVWAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode==EPCGExBinSeedMode::PositionConstant", EditConditionHides))
	FVector SeedPosition = FVector(0, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedMode==EPCGExBinSeedMode::PositionAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SeedPositionAttribute;

	/** The stacking side is the axis that will generate the smallest free space for further insertion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExAxis StackingSide = EPCGExAxis::Up;

	/** Open side are the two free spaces that "sandwich" the selected stacking side. o.e when stacking over Up or Down, open sides are left & right, enabling switch will use front & back instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bSwitchOpenSides = false;

	/** If enabled, fitting will try to avoid wasted space by not creating free spaces that are below a certain threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bAvoidWastedSpace = true;

	/** If enabled, fitting will try to avoid wasted space by not creating free spaces that are below a certain threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExPlacementFavor PlacementFavor = EPCGExPlacementFavor::SeedProximity;

	// TODO : Testable Rotation

	/** Occupation padding source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	EPCGExInputValueType OccupationPaddingInput = EPCGExInputValueType::Constant;

	/** Occupation padding attribute -- Will be broadcast to FVector. Occupation padding is an amount by which the bounds of a placed point will be expanded by after placement. This yield to greater fragmentation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, DisplayName="Occupation Padding (Attr)", EditCondition="OccupationPaddingInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OccupationPaddingAttribute;

	/** Occupation padding. Occupation padding is an amount by which the bounds of a placed point will be expanded by after placement. This yield to greater fragmentation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable, DisplayName="Occupation Padding", EditCondition="OccupationPaddingInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector OccupationPadding = FVector::ZeroVector;

	/** If enabled, the padding will not be relative (rotated) if the item is rotated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta = (PCG_Overridable))
	bool bAbsolutePadding = true;

	/** If enabled, won't throw a warning if there are more bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietTooManyBinsWarning = false;

	/** If enabled, won't throw a warning if there are fewer bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietTooFewBinsWarning = false;

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;
};

struct FPCGExBinPackingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBinPackingElement;
	TSharedPtr<PCGExData::FPointIOCollection> Bins;
	TArray<FPCGExUVW> BinsUVW;

	TSharedPtr<PCGExData::FPointIOCollection> Discarded;
};

class FPCGExBinPackingElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBinPacking
{
	using PCGExLayout::FItem;
	using PCGExLayout::FSpace;

	class FBin : public TSharedFromThis<FBin>
	{
	protected:
		double MaxVolume = 0;
		double MaxDist = 0;
		FVector Seed = FVector::ZeroVector;

		TArray<FSpace> Spaces;
		void AddSpace(const FBox& InBox);

	public:
		int32 MaxItems = 0;
		FBox Bounds;
		FTransform Transform;

		const UPCGExBinPackingSettings* Settings = nullptr;
		FVector WastedSpaceThresholds = FVector::ZeroVector;
		TArray<FItem> Items;

		explicit FBin(const FPCGPoint& InBinPoint, const FVector& InSeed);
		~FBin() = default;

		bool IsFull() const { return Items.Num() <= MaxItems; }
		int32 GetBestSpaceScore(const FItem& InItem, double& OutScore, FRotator& OutRotator) const;
		void AddItem(int32 SpaceIndex, FItem& InItem);
		bool Insert(FItem& InItem);
		void UpdatePoint(FPCGPoint& InPoint, const FItem& InItem) const;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBinPackingContext, UPCGExBinPackingSettings>
	{
	protected:
		double MinOccupation = 0;
		TSharedPtr<PCGExSorting::PointSorter<true>> Sorter;
		TArray<TSharedPtr<FBin>> Bins;
		TBitArray<> Fitted;
		TSharedPtr<PCGExData::TBuffer<FVector>> PaddingBuffer;
		bool bHasUnfitted = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
			bInlineProcessPoints = true;
		}

		virtual ~FProcessor() override
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}

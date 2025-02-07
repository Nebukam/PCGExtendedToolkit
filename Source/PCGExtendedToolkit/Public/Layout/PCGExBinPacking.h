// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Transform/PCGExTransform.h"


#include "PCGExBinPacking.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBinPackingSettings : public UPCGExPointsProcessorSettings
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

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Controls the order in which points will be sorted, when using sorting rules. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Per-bin center of gravity. Represent a bound-relative location to start packing from. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExUVW BinCenter = FPCGExUVW();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector BinUVW = FVector(0, 0, -1);

	/** If enabled, won't throw a warning if there are more bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietTooManyBinsWarning = false;

	/** If enabled, won't throw a warning if there are fewer bins than there are inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietTooFewBinsWarning = false;

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBinPackingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBinPackingElement;
	TSharedPtr<PCGExData::FPointIOCollection> Bins;
	TArray<FPCGExUVW> BinsUVW;

	TSharedPtr<PCGExData::FPointIOCollection> Discarded;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBinPackingElement final : public FPCGExPointsProcessorElement
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
	struct FBinItem
	{
		int32 Index = 0;
		FBox Box = FBox(ForceInit);
	};

	class FBin : public TSharedFromThis<FBin>
	{
	public:
		FVector CenterOfGravity = FVector::ZeroVector;
		FBox BinBounds;
		FTransform Transform;
		TArray<FBinItem> PlacedItems;
		TArray<FBox> FreeSpaces;

		explicit FBin(const FPCGPoint& InBinPoint);
		~FBin() = default;

		void AddItem(int32 SpaceIndex, FBinItem& InItem);
		bool Insert(FBinItem& InItem);
		void UpdatePoint(FPCGPoint& InPoint, const FBinItem& InItem) const;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBinPackingContext, UPCGExBinPackingSettings>
	{
		TSharedPtr<PCGExSorting::PointSorter<true>> Sorter;
		TArray<TSharedPtr<FBin>> Bins;
		TBitArray<> Fitted;
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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}

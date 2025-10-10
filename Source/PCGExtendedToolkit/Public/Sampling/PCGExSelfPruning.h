﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMathMean.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSorting.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointFilter.h"
#include "Details/PCGExSettingsMacros.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExSelfPruning.generated.h"

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

UENUM()
enum class EPCGExSelfPruningMode : uint8
{
	Prune       = 0 UMETA(DisplayName = "Prune", ToolTip="Prune points"),
	WriteResult = 1 UMETA(DisplayName = "Write Result", ToolTip="Write the number of overlaps"),
};

UENUM()
enum class EPCGExSelfPruningExpandOrder : uint8
{
	None   = 0 UMETA(DisplayName = "None", ToolTip="Do not expand bounds"),
	Before = 1 UMETA(DisplayName = "Before Transform", ToolTip="Expand bounds before world transform"),
	After  = 2 UMETA(DisplayName = "After Transform", ToolTip="Expand bounds after world transform"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="sampling/self-pruning"))
class UPCGExSelfPruningSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SelfPruning, "Self Pruning", "A slower, more precise self pruning node.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorFilter); }
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
#endif

	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters which points can be processed as overlapping", PCGExFactories::PointFilters, false)

protected:
	virtual bool HasDynamicPins() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Whether to prune points or write the number of overlaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExSelfPruningMode Mode = EPCGExSelfPruningMode::Prune;

	/** Whether to sort hash components or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Sort Direction"))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Sort over a random per-point value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Randomize", EditCondition="Mode == EPCGExSelfPruningMode::Prune", EditConditionHides))
	bool bRandomize = true;

	/** Sort over a random per-point value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ─└─ Range", EditCondition="bRandomize && Mode == EPCGExSelfPruningMode::Prune", EditConditionHides, ClampMin=0, ClampMax=1))
	double RandomRange = 0.05;

	/** Name of the attribute to write the number of overlap to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Output to", EditCondition="Mode == EPCGExSelfPruningMode::WriteResult", EditConditionHides))
	FName NumOverlapAttributeName = FName("NumOverlaps");

	/** Discrete mode write the number as-is, relative will normalize against the highest number of overlaps found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Units", EditCondition="Mode == EPCGExSelfPruningMode::WriteResult", EditConditionHides))
	EPCGExMeanMeasure Units = EPCGExMeanMeasure::Discrete;

	/** Whether to do a OneMinus on the normalized overlap count value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="   └─ OneMinus", EditCondition="Mode == EPCGExSelfPruningMode::WriteResult && Units == EPCGExMeanMeasure::Relative", EditConditionHides))
	bool bOutputOneMinusOverlap = false;

	/** If enabled, does very precise and EXPENSIVE spatial tests. Only supported for pruning. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_NotOverridable))
	bool bPreciseTest = false;
	
	/** If and how to expand the primary bounds (bounds used for the main point being evaluated) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_NotOverridable))
	EPCGExSelfPruningExpandOrder PrimaryMode = EPCGExSelfPruningExpandOrder::None;

	/** Type of primary expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" ├─ Input", EditCondition="PrimaryMode != EPCGExSelfPruningExpandOrder::None", EditConditionHides))
	EPCGExInputValueType PrimaryExpansionInput = EPCGExInputValueType::Constant;

	/** Primary Expansion value. Uniform, discrete offset applied to bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" └─ Primary Expansion (Attr)", EditCondition="PrimaryMode != EPCGExSelfPruningExpandOrder::None && PrimaryExpansionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector PrimaryExpansionAttribute;

	/** Primary Expansion value. Uniform, discrete offset applied to bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" └─ Primary Expansion", EditCondition="PrimaryMode != EPCGExSelfPruningExpandOrder::None && PrimaryExpansionInput == EPCGExInputValueType::Constant", EditConditionHides))
	double PrimaryExpansion = 0;

	PCGEX_SETTING_VALUE_DECL(PrimaryExpansion, double)


	/** If and how to expand the primary bounds (bounds used for neighbors points against the main point being evaluated) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_NotOverridable))
	EPCGExSelfPruningExpandOrder SecondaryMode = EPCGExSelfPruningExpandOrder::None;

	/** Type of secondary expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" ├─ Input", EditCondition="SecondaryMode != EPCGExSelfPruningExpandOrder::None", EditConditionHides))
	EPCGExInputValueType SecondaryExpansionInput = EPCGExInputValueType::Constant;

	/** Secondary Expansion value. Uniform, discrete offset applied to bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" └─ Secondary Expansion (Attr)", EditCondition="SecondaryMode != EPCGExSelfPruningExpandOrder::None && SecondaryExpansionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SecondaryExpansionAttribute;

	/** Secondary Expansion value. Uniform, discrete offset applied to bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Expansion", meta=(PCG_Overridable, DisplayName=" └─ Secondary Expansion", EditCondition="SecondaryMode != EPCGExSelfPruningExpandOrder::None && SecondaryExpansionInput == EPCGExInputValueType::Constant", EditConditionHides))
	double SecondaryExpansion = 0;

	PCGEX_SETTING_VALUE_DECL(SecondaryExpansion, double)
};

struct FPCGExSelfPruningContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSelfPruningElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExSelfPruningElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SelfPruning)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSelfPruning
{
	struct FCandidateInfos
	{
		FCandidateInfos() = default;

		int32 Index = -1;
		int32 Overlaps = 0;
		int8 bSkip = 0;
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSelfPruningContext, UPCGExSelfPruningSettings>
	{
	protected:
		TSharedPtr<PCGExDetails::TSettingValue<double>> PrimaryExpansion;
		TSharedPtr<PCGExDetails::TSettingValue<double>> SecondaryExpansion;

		TBitArray<> Mask;
		TArray<int32> Priority;
		TArray<FCandidateInfos> Candidates;
		TArray<FBox> BoxSecondary;

		int32 LastCandidatesCount = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;

		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void CompleteWork() override;
	};
}

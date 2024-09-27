// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExUberFilterCollections.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Uber Filter Mode"))
enum class EPCGExUberFilterCollectionsMode : uint8
{
	All     = 0 UMETA(DisplayName = "All", ToolTip="All points must pass the filters."),
	Any     = 1 UMETA(DisplayName = "Any", ToolTip="At least one point must pass the filter."),
	Partial = 2 UMETA(DisplayName = "Partial", ToolTip="A given amount of points must pass the filter."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExUberFilterCollectionsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
#if WITH_EDITOR

public:
#endif
	//~End UObject interface

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UberFilterCollections, "Uber Filter (Collection)", "Filter entire collections based on multiple rules & conditions.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilterHub; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, true)
	//~End UPCGExPointsProcessorSettings

public:
	/** Write result to point instead of split outputs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExUberFilterCollectionsMode Mode = EPCGExUberFilterCollectionsMode::All;

	/** Partial value type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExUberFilterCollectionsMode::Partial", EditConditionHides))
	EPCGExMeanMeasure Measure = EPCGExMeanMeasure::Relative;

	/** Partial value comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExUberFilterCollectionsMode::Partial", EditConditionHides))
	EPCGExComparison Comparison = EPCGExComparison::EqualOrGreater;

	/** Partial value type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExUberFilterCollectionsMode::Partial && Measure==EPCGExMeanMeasure::Relative", EditConditionHides, ClampMin=0, ClampMax=1))
	double DblThreshold = 0.5;

	/** Partial value type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExUberFilterCollectionsMode::Partial && Measure==EPCGExMeanMeasure::Discrete", EditConditionHides, ClampMin=0))
	int32 IntThreshold = 10;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;

	/** Invert the filter result */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSwap = false;

private:
	friend class FPCGExUberFilterCollectionsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExUberFilterCollectionsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExUberFilterCollectionsElement;

	TSharedPtr<PCGExData::FPointIOCollection> Inside;
	TSharedPtr<PCGExData::FPointIOCollection> Outside;

	int32 NumPairs = 0;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExUberFilterCollectionsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExUberFilterCollections
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExUberFilterCollectionsContext, UPCGExUberFilterCollectionsSettings>
	{
		int32 NumPoints = 0;
		int32 NumInside = 0;
		int32 NumOutside = 0;

	public:
		TSharedPtr<PCGExData::FPointIO> Inside;
		TSharedPtr<PCGExData::FPointIO> Outside;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void Output() override;
	};
}

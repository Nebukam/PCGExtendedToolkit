// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "PCGExSplitPath.generated.h"

namespace PCGExSplitPath
{
	const FName SourceSplitFilters = TEXT("Split Conditions");
}

UENUM()
enum class EPCGExPathSplitAction : uint8
{
	Split      = 0 UMETA(DisplayName = "Split", ToolTip="Duplicate the split point so the original becomes a new end, and the copy a new start."),
	Remove     = 1 UMETA(DisplayName = "Remove", ToolTip="Remove the split point, shrinking both the previous and next paths."),
	Disconnect = 2 UMETA(DisplayName = "Disconnect", ToolTip="Disconnect the split point from the next one, starting a new path from the next."),
	Partition  = 3 UMETA(DisplayName = "Partition", ToolTip="Works like split but only create new data set as soon as the filter result changes from its previous result."),
	Switch     = 4 UMETA(DisplayName = "Switch", ToolTip="Use the result of the filter as a switch signal to change between keep/prune behavior."),
};

UENUM()
enum class EPCGExPathSplitInitialValue : uint8
{
	Constant          = 0 UMETA(DisplayName = "Constant", ToolTip="Use a constant value."),
	ConstantPreserve  = 1 UMETA(DisplayName = "Constant (Preserve)", ToolTip="Use a constant value, but does not switch if the first value is the same."),
	FromPoint         = 2 UMETA(DisplayName = "From Point", ToolTip="Use the first point starting value."),
	FromPointPreserve = 3 UMETA(DisplayName = "From Point (Preserve)", ToolTip="Use the first point starting value, but preserve its behavior."),
};

/**
 * 
 */
UCLASS()
class UPCGExSplitPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	// TODO : Display split mode in title
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSplit, "Path : Split", "Split existing paths into multiple new paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExSplitPath::SourceSplitFilters, "Filters used to know if a point should be split", PCGExFactories::PointFilters, true)
	//~End UPCGExPointsProcessorSettings

	/** If both split and remove are true, the selected behavior takes priority */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathSplitAction SplitAction = EPCGExPathSplitAction::Split;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SplitAction==EPCGExPathSplitAction::Switch || SplitAction==EPCGExPathSplitAction::Partition", EditConditionHides))
	EPCGExPathSplitInitialValue InitialBehavior = EPCGExPathSplitInitialValue::Constant;

	/** The initial switch value to start from. If false, will only starting to create paths after the first true result. If false, will start to create paths from the beginning and stop at the first true result instead.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InitialBehavior == EPCGExPathSplitInitialValue::Constant && (SplitAction==EPCGExPathSplitAction::Switch || SplitAction==EPCGExPathSplitAction::Partition)", EditConditionHides))
	bool bInitialValue = false;

	/** Should point insertion be inclusive of the behavior change */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SplitAction==EPCGExPathSplitAction::Switch || SplitAction==EPCGExPathSplitAction::Partition", EditConditionHides))
	bool bInclusive = false;

	/** Whether to output single-point data or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOmitSinglePointOutputs = true;

	/** When processing closed loop paths, paths that aren't looping anymore will be tagged. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta = (PCG_Overridable))
	FPCGExPathClosedLoopUpdateDetails UpdateTags;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfEvenSplit = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfEvenSplit"))
	FString IsEvenTag = TEXT("EvenSplit");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfOddSplit = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfOddSplit"))
	FString IsOddTag = TEXT("OddSplit");
};

struct FPCGExSplitPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSplitPathElement;

	FPCGExPathClosedLoopUpdateDetails UpdateTags;
};

class FPCGExSplitPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SplitPath)
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSplitPath
{
	struct PCGEXTENDEDTOOLKIT_API FPath
	{
		bool bEven = false;
		int32 Start = -1;
		int32 End = -1;
		int32 Count = 0;

		FPath()
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSplitPathContext, UPCGExSplitPathSettings>
	{
		bool bClosedLoop = false;

		TArray<FPath> Paths;
		TArray<TSharedPtr<PCGExData::FPointIO>> PathsIOs;

		bool bWrapLastPath = false;
		bool bAddOpenTag = false;
		int8 bLastResult = false;
		bool bEven = true;

		int32 LastIndex = -1;
		int32 CurrentPath = -1;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;

		void DoActionSplit(const int32 Index);
		void DoActionRemove(const int32 Index);
		void DoActionDisconnect(const int32 Index);
		void DoActionPartition(const int32 Index);
		void DoActionSwitch(const int32 Index);

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Output() override;
	};
}

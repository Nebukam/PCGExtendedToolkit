// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"

#include "PCGExSplitPath.generated.h"

namespace PCGExSplitPath
{
	const FName SourceSplitFilters = TEXT("SplitConditions");
}

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action")--E*/)
enum class EPCGExPathSplitAction : uint8
{
	Split      = 0 UMETA(DisplayName = "Split", ToolTip="Duplicate the split point so the original becomes a new end, and the copy a new start."),
	Remove     = 1 UMETA(DisplayName = "Remove", ToolTip="Remove the split point, shrinking both the previous and next paths."),
	Disconnect = 2 UMETA(DisplayName = "Disconnect", ToolTip="Disconnect the split point from the next one, starting a new path from the next."),
	Partition  = 3 UMETA(DisplayName = "Partition", ToolTip="Works like split but only create new data set as soon as the filter result changes from its previous result."),
	Switch     = 4 UMETA(DisplayName = "Switch", ToolTip="Use the result of the filter as a switch signal to change between keep/prune behavior."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Path Split Initial Value")--E*/)
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
UCLASS(/*E--MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path"--E*/)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplitPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSplit, "Path : Split", "Split existing paths into multiple new paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfEvenSplit = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfEvenSplit"))
	FString IsEvenTag = TEXT("EvenSplit");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfOddSplit = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfOddSplit"))
	FString IsOddTag = TEXT("OddSplit");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplitPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSplitPathElement;

	FPCGExPathClosedLoopUpdateDetails UpdateTags;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplitPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExSplitPath
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FPath
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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		FORCEINLINE void DoActionSplit(const int32 Index)
		{
			if (!PointFilterCache[Index])
			{
				if (CurrentPath == -1)
				{
					CurrentPath = Paths.Emplace();
					FPath& NewPath = Paths[CurrentPath];
					NewPath.Start = Index;
				}

				FPath& Path = Paths[CurrentPath];
				Path.Count++;
				return;
			}

			if (CurrentPath != -1)
			{
				FPath& ClosedPath = Paths[CurrentPath];
				ClosedPath.End = Index;
				ClosedPath.Count++;
			}

			CurrentPath = Paths.Emplace();
			FPath& NewPath = Paths[CurrentPath];
			NewPath.Start = Index;
			NewPath.Count++;
		}

		FORCEINLINE void DoActionRemove(const int32 Index)
		{
			if (!PointFilterCache[Index])
			{
				if (CurrentPath == -1)
				{
					CurrentPath = Paths.Emplace();
					FPath& NewPath = Paths[CurrentPath];
					NewPath.Start = Index;
				}

				FPath& Path = Paths[CurrentPath];
				Path.Count++;
				return;
			}

			if (CurrentPath != -1)
			{
				FPath& Path = Paths[CurrentPath];
				Path.End = Index - 1;
			}

			CurrentPath = -1;
		}

		FORCEINLINE void DoActionDisconnect(const int32 Index)
		{
			if (!PointFilterCache[Index])
			{
				if (CurrentPath == -1)
				{
					CurrentPath = Paths.Emplace();
					FPath& NewPath = Paths[CurrentPath];
					NewPath.Start = Index;
				}

				FPath& Path = Paths[CurrentPath];
				Path.Count++;
				return;
			}

			if (CurrentPath != -1)
			{
				FPath& ClosedPath = Paths[CurrentPath];
				ClosedPath.End = Index;
				ClosedPath.Count++;
			}

			CurrentPath = -1;
		}

		FORCEINLINE void DoActionPartition(const int32 Index)
		{
			if (PointFilterCache[Index] != bLastResult)
			{
				bLastResult = !bLastResult;

				if (CurrentPath != -1)
				{
					FPath& ClosedPath = Paths[CurrentPath];
					if (Settings->bInclusive)
					{
						ClosedPath.End = Index;
						ClosedPath.Count++;
					}
					else
					{
						ClosedPath.End = Index - 1;
					}

					CurrentPath = -1;
				}
			}

			if (CurrentPath == -1)
			{
				CurrentPath = Paths.Emplace();
				FPath& NewPath = Paths[CurrentPath];
				NewPath.bEven = bEven;
				bEven = !bEven;
				NewPath.Start = Index;
			}

			FPath& Path = Paths[CurrentPath];
			Path.Count++;
		}

		FORCEINLINE void DoActionSwitch(const int32 Index)
		{
			auto ClosePath = [&]()
			{
				if (CurrentPath != -1)
				{
					FPath& ClosedPath = Paths[CurrentPath];
					if (Settings->bInclusive)
					{
						ClosedPath.End = Index;
						ClosedPath.Count++;
					}
					else
					{
						ClosedPath.End = Index - 1;
					}
				}

				CurrentPath = -1;
			};

			if (PointFilterCache[Index]) { bLastResult = !bLastResult; }

			if (bLastResult)
			{
				if (CurrentPath == -1)
				{
					CurrentPath = Paths.Emplace();
					FPath& NewPath = Paths[CurrentPath];
					NewPath.Start = Index;
				}

				FPath& Path = Paths[CurrentPath];
				Path.Count++;
				return;
			}

			ClosePath();
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Output() override;
	};
}

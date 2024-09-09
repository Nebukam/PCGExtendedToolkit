// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExSplitPath.generated.h"

namespace PCGExSplitPath
{
	const FName SourceSplitFilters = TEXT("SplitConditions");
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action"))
enum class EPCGExPathSplitAction : uint8
{
	Split      = 0 UMETA(DisplayName = "Split", ToolTip="Duplicate the split point so the original becomes a new end, and the copy a new start."),
	Remove     = 1 UMETA(DisplayName = "Remove", ToolTip="Remove the split point, shrinking both the previous and next paths."),
	Disconnect = 2 UMETA(DisplayName = "Disconnect", ToolTip="Disconnect the split point from the next one, starting a new path from the next."),
	Partition  = 3 UMETA(DisplayName = "Partition", ToolTip="Works like split but only create new data set as soon as the filter result changes from its previous result."),
	Switch     = 4 UMETA(DisplayName = "Switch", ToolTip="Use the result of the filter as a switch signal to change between keep/prune behavior."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplitPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SplitPath, "Path : Split", "Split existing paths into multiple new paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExSplitPath::SourceSplitFilters, "Filters used to know if a point should be split", PCGExFactories::PointFilters, true)
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** When closed path is enabled, paths that aren't closed anymore will be tagged. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bClosedPath", EditConditionHides))
	FString OpenPathTag = "Open";

	/** If both split and remove are true, the selected behavior takes priority */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathSplitAction SplitAction = EPCGExPathSplitAction::Split;

	/** The initial switch value to start from. If false, will only starting to create paths after the first true result. If false, will start to create paths from the beginning and stop at the first true result instead.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SplitAction==EPCGExPathSplitAction::Switch", EditConditionHides))
	bool bInitialSwitchValue = false;

	/** Should point insertion be inclusive of the behavior change */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SplitAction==EPCGExPathSplitAction::Switch || SplitAction==EPCGExPathSplitAction::Partition", EditConditionHides))
	bool bInclusive = false;

	/** Whether to output single-point data or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOmitSinglePointOutputs = true;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplitPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExSplitPathElement;

	virtual ~FPCGExSplitPathContext() override;

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

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExSplitPathContext* LocalTypedContext = nullptr;
		const UPCGExSplitPathSettings* LocalSettings = nullptr;

		bool bClosedPath = false;

		TArray<FPath> Paths;
		TArray<PCGExData::FPointIO*> PathsIOs;

		bool bWrapLastPath = false;
		bool bAddOpenTag = false;
		bool bLastResult = false;
		bool bEven = true;

		int32 LastIndex = -1;
		int32 CurrentPath = -1;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;

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
					if (LocalSettings->bInclusive)
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
					if (LocalSettings->bInclusive)
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

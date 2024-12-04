// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplitPath.h"

#include "Data/PCGExPointFilter.h"


#define LOCTEXT_NAMESPACE "PCGExSplitPathElement"
#define PCGEX_NAMESPACE SplitPath

#if WITH_EDITOR
void UPCGExSplitPathSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (SplitAction == EPCGExPathSplitAction::Switch)
	{
	}
}
#endif

PCGExData::EIOInit UPCGExSplitPathSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(SplitPath)

bool FPCGExSplitPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	PCGEX_FWD(UpdateTags)
	Context->UpdateTags.Init();

	Context->MainPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainPaths->OutputPin = Settings->GetMainOutputPin();

	return true;
}

bool FPCGExSplitPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplitPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitSinglePointOutputs) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
					else { bHasInvalidInputs = true; }
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to split."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPaths->Pairs.Reserve(Context->MainPaths->Pairs.Num() + Context->MainBatch->GetNumProcessors());
	Context->MainBatch->Output();

	Context->MainPaths->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSplitPath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSplitPath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);

		const int32 NumPoints = PointDataFacade->GetNum();
		const int32 ChunkSize = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize();

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, TaskGroup)

#define PCGEX_SPLIT_ACTION(_NAME)\
		TaskGroup->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx){\
					PCGEX_ASYNC_THIS \
					This->PointDataFacade->Fetch(StartIndex, Count);\
					This->FilterScope(StartIndex, Count);\
					PCGEX_ASYNC_SUB_LOOP { This->_NAME(i); } };

		if (Settings->SplitAction == EPCGExPathSplitAction::Partition ||
			Settings->SplitAction == EPCGExPathSplitAction::Switch)
		{
			PointDataFacade->Fetch(0, 1);
			FilterScope(0, 1);

			switch (Settings->InitialBehavior)
			{
			default:
			case EPCGExPathSplitInitialValue::Constant:
				bLastResult = static_cast<int8>(Settings->bInitialValue);
				break;
			case EPCGExPathSplitInitialValue::ConstantPreserve:
				bLastResult = static_cast<int8>(Settings->bInitialValue) == PointFilterCache[0] ? !bLastResult : bLastResult;
				break;
			case EPCGExPathSplitInitialValue::FromPoint:
				bLastResult = PointFilterCache[0];
				break;
			case EPCGExPathSplitInitialValue::FromPointPreserve:
				bLastResult = !PointFilterCache[0];
				break;
			}
		}

		switch (Settings->SplitAction)
		{
		case EPCGExPathSplitAction::Split:
			PCGEX_SPLIT_ACTION(DoActionSplit)
			break;
		case EPCGExPathSplitAction::Remove:
			PCGEX_SPLIT_ACTION(DoActionRemove)
			break;
		case EPCGExPathSplitAction::Disconnect:
			PCGEX_SPLIT_ACTION(DoActionDisconnect)
			break;
		case EPCGExPathSplitAction::Partition:
			PCGEX_SPLIT_ACTION(DoActionPartition)
			break;
		case EPCGExPathSplitAction::Switch:
			PCGEX_SPLIT_ACTION(DoActionSwitch)
			break;
		default: ;
		}

#undef PCGEX_SPLIT_ACTION

		TaskGroup->StartSubLoops(NumPoints, ChunkSize, true);

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const FPath& PathInfos = Paths[Iteration];

		//if (PathInfos.Count < 1 || PathInfos.Start == -1) { return; }                                    // This should never happen
		//if (PathInfos.End == -1 && (PathInfos.Start + PathInfos.Count) != PointIO->GetNum()) { return; } // This should never happen

		if (Iteration == 0 && bWrapLastPath) { return; }

		const bool bWrapWithStart = PathInfos.End == -1 && bWrapLastPath;
		const int32 NumPathPoints = bWrapWithStart ? PathInfos.Count + Paths[0].Count : PathInfos.Count;

		if (NumPathPoints == 1 && Settings->bOmitSinglePointOutputs) { return; }

		const TSharedPtr<PCGExData::FPointIO> PathIO = NewPointIO(PointDataFacade->Source);
		PathIO->InitializeOutput(PCGExData::EIOInit::New);
		PathsIOs[Iteration] = PathIO;

		const TArray<FPCGPoint>& OriginalPoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		PCGEx::InitArray(MutablePoints, NumPathPoints);

		for (int i = 0; i < PathInfos.Count; i++) { MutablePoints[i] = OriginalPoints[PathInfos.Start + i]; }

		if (bWrapWithStart) // There was a cut somewhere in the closed path.
		{
			const FPath& StartPathInfos = Paths[0];
			for (int i = 0; i < StartPathInfos.Count; i++) { MutablePoints[PathInfos.Count + i] = OriginalPoints[StartPathInfos.Start + i]; }
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Paths.IsEmpty()) { return; }

		if (bClosedLoop)
		{
			if (Paths.Num() >= 2) { bWrapLastPath = Paths[0].Start == 0 && Paths.Last().End == -1; }
			if (Paths.Num() > 1 || Paths[0].End != -1 || Paths[0].Start != 0) { bAddOpenTag = true; }
		}

		PathsIOs.Init(nullptr, Paths.Num());

		StartParallelLoopForRange(Paths.Num());

		//TODO : If closed path is enabled, and if the first & last points are not removed after the split
		//re-order them and join them so as to recreate the "missing" edge
	}

	void FProcessor::Output()
	{
		int32 OddEven = 0;
		for (const TSharedPtr<PCGExData::FPointIO>& PathIO : PathsIOs)
		{
			if (!PathIO) { continue; }
			if (bAddOpenTag) { Context->UpdateTags.Update(PathIO); }

			if ((OddEven & 1) == 0) { if (Settings->bTagIfEvenSplit) { PathIO->Tags->Add(Settings->IsEvenTag); } }
			else if (Settings->bTagIfOddSplit) { PathIO->Tags->Add(Settings->IsOddTag); }
			Context->MainPaths->AddUnsafe(PathIO);
			OddEven++;
		}

		PathsIOs.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

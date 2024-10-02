// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplitPath.h"

#include "Data/PCGExPointFilter.h"


#define LOCTEXT_NAMESPACE "PCGExSplitPathElement"
#define PCGEX_NAMESPACE SplitPath

PCGExData::EInit UPCGExSplitPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(SplitPath)

bool FPCGExSplitPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	PCGEX_FWD(UpdateTags)
	Context->UpdateTags.Init();

	Context->MainPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExSplitPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplitPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitSinglePointOutputs) { Entry->InitializeOutput(Context, PCGExData::EInit::Forward); }
					else { bHasInvalidInputs = true; }
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to split."));
			return true;
		}

		if (bHasInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPaths->Pairs.Reserve(Context->MainPaths->Pairs.Num() + Context->MainBatch->GetNumProcessors());
	Context->MainBatch->Output();

	Context->MainPaths->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSplitPath
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSplitPath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);

		const int32 NumPoints = PointDataFacade->GetNum();
		const int32 ChunkSize = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize();

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, TaskGroup)
		TaskGroup->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
				FilterScope(StartIndex, Count);
			};

		switch (Settings->SplitAction)
		{
		case EPCGExPathSplitAction::Split:
			TaskGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionSplit(Index); };

			break;
		case EPCGExPathSplitAction::Remove:
			TaskGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionRemove(Index); };
			break;
		case EPCGExPathSplitAction::Disconnect:
			TaskGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionDisconnect(Index); };
			break;
		case EPCGExPathSplitAction::Partition:
			PointDataFacade->Fetch(0, 1);
			bLastResult = PrimaryFilters->Test(0);
			TaskGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionPartition(Index); };
			break;
		case EPCGExPathSplitAction::Switch:
			bLastResult = Settings->bInitialSwitchValue;
			TaskGroup->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionSwitch(Index); };
			break;
		default: ;
		}

		TaskGroup->StartIterations(NumPoints, ChunkSize, true);

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

		TSharedPtr<PCGExData::FPointIO> PathIO = MakeShared<PCGExData::FPointIO>(ExecutionContext, PointDataFacade->Source);
		PathIO->InitializeOutput(Context, PCGExData::EInit::NewOutput);
		PathsIOs[Iteration] = PathIO;

		const TArray<FPCGPoint>& OriginalPoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		PCGEx::InitArray(MutablePoints, NumPathPoints);

		for (int i = 0; i < PathInfos.Count; ++i) { MutablePoints[i] = OriginalPoints[PathInfos.Start + i]; }

		if (bWrapWithStart) // There was a cut somewhere in the closed path.
		{
			const FPath& StartPathInfos = Paths[0];
			for (int i = 0; i < StartPathInfos.Count; ++i) { MutablePoints[PathInfos.Count + i] = OriginalPoints[StartPathInfos.Start + i]; }
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
		for (const TSharedPtr<PCGExData::FPointIO>& PathIO : PathsIOs)
		{
			if (!PathIO) { continue; }
			if (bAddOpenTag) { Context->UpdateTags.Update(PathIO); }
			Context->MainPaths->AddUnsafe(PathIO);
		}

		PathsIOs.Empty(); // So they don't get deleted
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

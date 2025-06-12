// Copyright 2025 Timothé Lapetite and contributors
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

PCGEX_INITIALIZE_ELEMENT(SplitPath)

bool FPCGExSplitPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

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
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSplitPath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSplitPath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());

		const int32 NumPoints = PointDataFacade->GetNum();
		const int32 ChunkSize = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize();

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, TaskGroup)

#define PCGEX_SPLIT_ACTION(_NAME)\
		TaskGroup->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope){\
					PCGEX_ASYNC_THIS \
					This->PointDataFacade->Fetch(Scope);\
					This->FilterScope(Scope);\
					PCGEX_SCOPE_LOOP(i) { This->_NAME(i); } };

		if (Settings->SplitAction == EPCGExPathSplitAction::Partition ||
			Settings->SplitAction == EPCGExPathSplitAction::Switch)
		{
			PointDataFacade->Fetch(PCGExMT::FScope(0, 1));
			FilterScope(PCGExMT::FScope(0, 1));

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

	void FProcessor::DoActionSplit(const int32 Index)
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

	void FProcessor::DoActionRemove(const int32 Index)
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

	void FProcessor::DoActionDisconnect(const int32 Index)
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

	void FProcessor::DoActionPartition(const int32 Index)
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

	void FProcessor::DoActionSwitch(const int32 Index)
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

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const FPath& PathInfos = Paths[Index];

			//if (PathInfos.Count < 1 || PathInfos.Start == -1) { continue; }                                    // This should never happen
			//if (PathInfos.End == -1 && (PathInfos.Start + PathInfos.Count) != PointIO->GetNum()) { continue; } // This should never happen

			if (Index == 0 && bWrapLastPath) { continue; }
			const bool bLastPath = PathInfos.End == -1;

			const bool bAppendStartPath = bWrapLastPath && bLastPath;
			int32 NumPathPoints = bAppendStartPath ? PathInfos.Count + Paths[0].Count : PathInfos.Count;
			int32 NumIterations = PathInfos.Count;

			if (!bAppendStartPath && bLastPath && bClosedLoop)
			{
				// First point added last
				NumPathPoints++;
				NumIterations++;
			}

			if (NumPathPoints == 1 && Settings->bOmitSinglePointOutputs) { continue; }

			const TSharedPtr<PCGExData::FPointIO> PathIO = NewPointIO(PointDataFacade->Source);
			PCGEX_INIT_IO_VOID(PathIO, PCGExData::EIOInit::New)

			PathsIOs[Index] = PathIO;

			const UPCGBasePointData* OriginalPoints = PointDataFacade->GetIn();
			UPCGBasePointData* MutablePoints = PathIO->GetOut();
			PCGEx::SetNumPointsAllocated(MutablePoints, NumPathPoints, OriginalPoints->GetAllocatedProperties());

			TArray<int32>& IdxMapping = PathIO->GetIdxMapping();

			const int32 IndexWrap = OriginalPoints->GetNumPoints();
			for (int i = 0; i < NumIterations; i++) { IdxMapping[i] = (PathInfos.Start + i) % IndexWrap; }

			if (bAppendStartPath)
			{
				// There was a cut somewhere in the closed path.
				const FPath& StartPathInfos = Paths[0];
				for (int i = 0; i < StartPathInfos.Count; i++) { IdxMapping[PathInfos.Count + i] = StartPathInfos.Start + i; }
			}

			PathIO->ConsumeIdxMapping(EPCGPointNativeProperties::All);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Paths.IsEmpty() || (Paths.Num() == 1 && Paths[0].Count == PointDataFacade->GetNum()))
		{
			// No splits, forward
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Forward)
			return;
		}

		if (bClosedLoop)
		{
			if (Paths.Num() > 1)
			{
				bWrapLastPath = Paths[0].Start == 0 && Paths.Last().End == -1 && !PointFilterCache[0];
			}

			if (Paths.Num() > 1 || Paths[0].End != -1 || Paths[0].Start != 0)
			{
				bAddOpenTag = true;
			}
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

			PCGExPaths::SetClosedLoop(PathIO->GetOut(), false);

			if ((OddEven & 1) == 0) { if (Settings->bTagIfEvenSplit) { PathIO->Tags->AddRaw(Settings->IsEvenTag); } }
			else if (Settings->bTagIfOddSplit) { PathIO->Tags->AddRaw(Settings->IsOddTag); }
			Context->MainPaths->Add_Unsafe(PathIO);
			OddEven++;
		}

		PathsIOs.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

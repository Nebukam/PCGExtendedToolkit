// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplitPath.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSplitPathElement"
#define PCGEX_NAMESPACE SplitPath

TArray<FPCGPinProperties> UPCGExSplitPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExSplitPath::SourceSplitFilters, "Filters used to know if a point should be split", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSplitPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(SplitPath)

FPCGExSplitPathContext::~FPCGExSplitPathContext()
{
	PCGEX_TERMINATE_ASYNC

	SplitFilterFactories.Empty();
	RemoveFilterFactories.Empty();
}

bool FPCGExSplitPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	GetInputFactories(Context, PCGExSplitPath::SourceSplitFilters, Context->SplitFilterFactories, PCGExFactories::PointFilters, false);

	if (Context->SplitFilterFactories.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing split filters"));
		return false;
	}

	Context->MainPaths = new PCGExData::FPointIOCollection(Context);
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExSplitPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplitPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitSinglePointOutputs) { Entry->InitializeOutput(PCGExData::EInit::Forward); }
					else { bHasInvalidInputs = true; }
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>* NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to split."));
			return true;
		}

		if (bHasInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPaths->Pairs.Reserve(Context->MainPaths->Pairs.Num() + Context->MainBatch->GetNumProcessors());
	Context->MainBatch->Output();

	Context->MainPaths->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSplitPath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(FilterManager)
		PCGEX_DELETE_TARRAY(PathsIOs)
		Paths.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSplitPath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SplitPath)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointDataFacade->bSupportsDynamic = true;
		bClosedPath = Settings->bClosedPath;

		if (!TypedContext->SplitFilterFactories.IsEmpty())
		{
			FilterManager = new PCGExPointFilter::TManager(PointDataFacade);
			if (!FilterManager->Init(Context, TypedContext->SplitFilterFactories)) { PCGEX_DELETE(FilterManager) }
		}

		if (!FilterManager)
		{
			// TODO : Throw error/warning
			return false;
		}

		const int32 NumPoints = PointIO->GetNum();
		const int32 ChunkSize = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize();

		PCGExMT::FTaskGroup* TaskGroup = AsyncManager->CreateGroup();
		TaskGroup->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { PointDataFacade->Fetch(StartIndex, Count); });

		switch (Settings->SplitAction)
		{
		case EPCGExPathSplitAction::Split:
			TaskGroup->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionSplit(Index); },
				NumPoints, ChunkSize, true);
			break;
		case EPCGExPathSplitAction::Remove:
			TaskGroup->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionRemove(Index); },
				NumPoints, ChunkSize, true);
			break;
		case EPCGExPathSplitAction::Disconnect:
			TaskGroup->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx) { DoActionDisconnect(Index); },
				NumPoints, ChunkSize, true);
			break;
		default: ;
		}

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

		if (NumPathPoints == 1 && LocalSettings->bOmitSinglePointOutputs) { return; }

		PCGExData::FPointIO* PathIO = new PCGExData::FPointIO(Context, PointIO);
		PathIO->InitializeOutput(PCGExData::EInit::NewOutput);
		PathsIOs[Iteration] = PathIO;

		const TArray<FPCGPoint>& OriginalPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, NumPathPoints);

		for (int i = 0; i < PathInfos.Count; i++) { MutablePoints[i] = OriginalPoints[PathInfos.Start + i]; }

		if (bWrapWithStart) // There was a cut somewhere in the closed path.
		{
			const FPath& StartPathInfos = Paths[0];
			for (int i = 0; i < StartPathInfos.Count; i++) { MutablePoints[PathInfos.Count + i] = OriginalPoints[StartPathInfos.Start + i]; }
		}
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SplitPath)

		if (Paths.IsEmpty()) { return; }

		if (bClosedPath)
		{
			if (Paths.Num() >= 2) { bWrapLastPath = Paths[0].Start == 0 && Paths.Last().End == -1; }
			if (Paths.Num() > 1 || Paths[0].End != -1 || Paths[0].Start != 0) { bAddOpenTag = Settings->OpenPathTag.IsEmpty() ? false : true; }
		}

		PCGEX_SET_NUM_NULLPTR(PathsIOs, Paths.Num())
		StartParallelLoopForRange(Paths.Num());

		//TODO : If closed path is enabled, and if the first & last points are not removed after the split
		//re-order them and join them so as to recreate the "missing" edge
	}

	void FProcessor::Output()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SplitPath)

		for (PCGExData::FPointIO* PathIO : PathsIOs)
		{
			if (!PathIO) { continue; }
			if (bAddOpenTag) { PathIO->Tags->RawTags.Add(Settings->OpenPathTag); }
			LocalTypedContext->MainPaths->AddUnsafe(PathIO);
		}

		PathsIOs.Empty(); // So they don't get deleted
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

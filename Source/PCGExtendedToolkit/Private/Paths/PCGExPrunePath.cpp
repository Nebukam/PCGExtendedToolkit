// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPrunePath.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExPrunePathElement"
#define PCGEX_NAMESPACE PrunePath

PCGExData::EInit UPCGExPrunePathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPrunePathSettings::GetPointFilterLabel() const { return FName("TriggerConditions"); }

PCGEX_INITIALIZE_ELEMENT(PrunePath)

FPCGExPrunePathContext::~FPCGExPrunePathContext()
{
	PCGEX_TERMINATE_ASYNC
}


bool FPCGExPrunePathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

	//PCGEX_FWD(bDoBlend)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExPrunePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPrunePathElement::Execute);

	PCGEX_CONTEXT(PrunePath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPrunePath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPrunePath::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainBatch->Output();
	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPrunePath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Outputs)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPrunePath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PrunePath)

		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		bClosedPath = Settings->bClosedPath;
		bCurrentSwitch = Settings->bInitialSwitchValue;

		if (!Settings->bGenerateNewPaths) { NewPathIO(); }

		bInlineProcessPoints = true;
		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		CachedIndex++;
		bool Trigger = LocalSettings->bInvertFilterValue ? !PointFilterCache[Index] : PointFilterCache[Index];

		if (LocalSettings->TriggerMode == EPCGExPathPruneTriggerMode::Switch)
		{
			if (Trigger) { bCurrentSwitch = !bCurrentSwitch; }
			Trigger = bCurrentSwitch;
		}

		if (Trigger)
		{
			if (LocalSettings->bGenerateNewPaths)
			{
				OutPoints = nullptr;
				CurrentPath = nullptr;
			}
			return;
		}

		if (!OutPoints)
		{
			CurrentPath = NewPathIO();
			if (Index == 0) { PathBegin = CurrentPath; }
		}

		FPCGPoint& NewPoint = OutPoints->Add_GetRef(Point);
		//OutMetadata->InitializeOnSet(NewPoint.MetadataEntry);
		LastValidIndex = Index;
	}

	PCGExData::FPointIO* FProcessor::NewPathIO()
	{
		// Create new point IO
		// TODO : Cache points IO locally and add them during the output step instead of this
		PCGExData::FPointIO* NewIO = new PCGExData::FPointIO(LocalTypedContext, PointIO);

		Outputs.Add(NewIO);

		OutPoints = &NewIO->GetOut()->GetMutablePoints();
		OutPoints->Reserve(PointIO->GetNum() - CachedIndex);

		OutMetadata = NewIO->GetOut()->Metadata;

		return NewIO;
	}

	void FProcessor::CompleteWork()
	{
		if (bClosedPath &&
			LocalSettings->bGenerateNewPaths &&
			PathBegin && CurrentPath && PathBegin != CurrentPath &&
			LastValidIndex == PointIO->GetNum() - 1)
		{
			// The last valid section connects back to the valid starting section
			// TODO : Insert PathBegin at the end of CurrentPath then get rid PathBegin
			// This will however put the first segment last in the outputs.

			TArray<FPCGPoint>& CurrentPoints = CurrentPath->GetOut()->GetMutablePoints();
			TArray<FPCGPoint>& BeginPoints = PathBegin->GetOut()->GetMutablePoints();

			const int32 NumBasePoints = CurrentPoints.Num();
			const int32 NumAddPoints = BeginPoints.Num();

			PCGEX_SET_NUM_UNINITIALIZED(CurrentPoints, NumBasePoints + NumAddPoints)

			for (int i = 0; i < NumAddPoints; i++)
			{
				const int32 Index = NumBasePoints + i;
				CurrentPoints[Index] = BeginPoints[i];
			}

			Outputs[0] = nullptr;
			PCGEX_DELETE(PathBegin);
		}
	}

	void FProcessor::Output()
	{
		for (PCGExData::FPointIO* IO : Outputs)
		{
			if (IO)
			{
				if (IO->GetNum(PCGExData::ESource::Out) > 0)
				{
					LocalTypedContext->MainPoints->AddUnsafe(IO);
				}
				else
				{
					PCGEX_DELETE(IO)
				}
			}
		}

		Outputs.Empty();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

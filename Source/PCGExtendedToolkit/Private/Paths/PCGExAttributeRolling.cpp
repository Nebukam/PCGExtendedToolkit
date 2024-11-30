// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExAttributeRolling.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRollingElement"
#define PCGEX_NAMESPACE AttributeRolling

UPCGExAttributeRollingSettings::UPCGExAttributeRollingSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

PCGExData::EIOInit UPCGExAttributeRollingSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)
	PCGEX_FWD(BlendingSettings)

	return true;
}


bool FPCGExAttributeRollingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRollingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAttributeRolling::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitInvalidPathsOutputs) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
					bHasInvalidInputs = true;
					return false;
				}

				Entry->InitializeOutput(PCGExData::EIOInit::Duplicate);
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAttributeRolling::FProcessor>>& NewBatch)
			{
				NewBatch->bPrefetchData = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainBatch->Output();
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAttributeRolling
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TPointsProcessor<FPCGExAttributeRollingContext, UPCGExAttributeRollingSettings>::RegisterBuffersDependencies(FacadePreloader);
		Settings->BlendingSettings.RegisterBuffersDependencies(Context, PointDataFacade, FacadePreloader);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeRolling::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		MaxIndex = PointDataFacade->GetNum(PCGExData::ESource::In) - 1;
		LastTriggerIndex = Settings->bReverseRolling ? MaxIndex : 0;

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Context->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade, PCGExData::ESource::In, true, nullptr);

		OutMetadata = PointDataFacade->GetOut()->Metadata;
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();

		bInlineProcessRange = true;

		if (Settings->TriggerAction == EPCGExRollingTriggerMode::None)
		{
			StartParallelLoopForRange(PointDataFacade->GetNum());
		}
		else
		{
			if (Context->FilterFactories.IsEmpty())
			{
				StartParallelLoopForRange(PointDataFacade->GetNum());
			}
			else
			{
				PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FilterTask)

				TWeakPtr<FProcessor> WeakThisPtr = SharedThis(this);

				FilterTask->OnCompleteCallback =
					[WeakThisPtr]()
					{
						const TSharedPtr<FProcessor> This = WeakThisPtr.Pin();
						if (!This) { return; }
						This->StartParallelLoopForRange(This->PointDataFacade->GetNum());
					};

				FilterTask->OnSubLoopStartCallback =
					[WeakThisPtr](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						const TSharedPtr<FProcessor> This = WeakThisPtr.Pin();
						if (!This) { return; }
						This->PrepareSingleLoopScopeForPoints(StartIndex, Count);
					};

				FilterTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			}
		}


		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		int32 TargetIndex = -1;
		int32 PrevIndex = -1;

		if (Settings->bReverseRolling)
		{
			TargetIndex = MaxIndex - Iteration;
			PrevIndex = TargetIndex + 1;
			if (TargetIndex == MaxIndex) { return; }
		}
		else
		{
			TargetIndex = Iteration;
			PrevIndex = TargetIndex - 1;
			if (TargetIndex == 0) { return; }
		}

		switch (Settings->TriggerAction)
		{
		default:
		case EPCGExRollingTriggerMode::None:
			break;
		case EPCGExRollingTriggerMode::Reset:
			if (PointFilterCache[TargetIndex]) { return; }
			break;
		case EPCGExRollingTriggerMode::Pin:
			if (PointFilterCache[TargetIndex]) { LastTriggerIndex = TargetIndex; }
			PrevIndex = LastTriggerIndex;
			break;
		}

		MetadataBlender->PrepareForBlending(TargetIndex);
		MetadataBlender->Blend(PrevIndex, TargetIndex, TargetIndex, 1);
		MetadataBlender->CompleteBlending(TargetIndex, 2, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

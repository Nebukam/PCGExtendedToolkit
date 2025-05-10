// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExAttributeRolling.h"

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRollingElement"
#define PCGEX_NAMESPACE AttributeRolling

UPCGExAttributeRollingSettings::UPCGExAttributeRollingSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

TArray<FPCGPinProperties> UPCGExAttributeRollingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)
	PCGEX_FWD(BlendingSettings)

	PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);


	// Rework : condition & action management.
	// Trigger Conditions capture the reference index to use for blending
	// - Trigger : Capture Index
	
	// Roll Mode :
	// - Start & Stop
	//		Separate start filters for start & stop trigger.
	//		Start enable rolling until a stop condition is met.
	//		- Start AND Stop handling : Start | Stop
	// - Toggle
	//		Toggle Conditions enable or disable rolling (blending) from that point on.

	// Reference Element Mode :
	// (define which index will be used for rolling the current point)
	// - Current-1 (Previous)
	// - Start (Start don't get rolled, but is rolled instead)
	// - Start-1 (Start will get rolled with previous value)
	// - Start-1 then Start (Start will get rolled with previous value, then becomes the new reference index)

	// [x] Roll on End
	//		If enabled, the point that stops rolling (toggle or stop) will have its values rolled over
	//		Otherwise, stopping points are not rolled on.
	
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
				PCGEX_SKIP_INVALID_PATH_ENTRY
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

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			Factory->RegisterBuffersDependencies(Context, FacadePreloader);
		}

		Settings->BlendingSettings.RegisterBuffersDependencies(Context, PointDataFacade, FacadePreloader);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeRolling::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		BlendOps = MakeShared<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>>();
		BlendOps->Reserve(Context->BlendingFactories.Num());

		if (!PCGExDataBlending::PrepareBlendOps(Context, PointDataFacade, Context->BlendingFactories, BlendOps))
		{
			return false;
		}

		MaxIndex = PointDataFacade->GetNum(PCGExData::ESource::In) - 1;
		LastTriggerIndex = Settings->bReverseRolling ? MaxIndex : 0;

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Context->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade, PCGExData::ESource::In, true, nullptr);

		OutMetadata = PointDataFacade->GetOut()->Metadata;
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();

		bDaisyChainProcessRange = true;

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

				FilterTask->OnCompleteCallback =
					[PCGEX_ASYNC_THIS_CAPTURE]()
					{
						PCGEX_ASYNC_THIS
						This->StartParallelLoopForRange(This->PointDataFacade->GetNum());
					};

				FilterTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						This->PrepareSingleLoopScopeForPoints(Scope);
					};

				FilterTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			}
		}
		
		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
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

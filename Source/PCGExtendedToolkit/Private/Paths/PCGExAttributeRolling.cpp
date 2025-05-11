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
	bSupportClosedLoops = true;
}

TArray<FPCGPinProperties> UPCGExAttributeRollingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (Mode == EPCGExRollingMode::StartStop)
	{
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceStartConditionLabel, "...", Required, {})
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceStopConditionLabel, "...", Required, {})
	}
	else
	{
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceToggleConditionLabel, "...", Required, {})
	}

	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Required, {})

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)

	if (Settings->Mode == EPCGExRollingMode::StartStop)
	{
		if (!PCGExFactories::GetInputFactories<UPCGExFilterFactoryData>(
			Context, PCGExPointFilter::SourceStartConditionLabel, Context->StartFilterFactories,
			PCGExFactories::PointFilters, true))
		{
			return false;
		}

		if (!PCGExFactories::GetInputFactories<UPCGExFilterFactoryData>(
			Context, PCGExPointFilter::SourceStopConditionLabel, Context->StopFilterFactories,
			PCGExFactories::PointFilters, true))
		{
			return false;
		}
	}
	else
	{
		if (!PCGExFactories::GetInputFactories<UPCGExFilterFactoryData>(
			Context, PCGExPointFilter::SourceToggleConditionLabel, Context->StartFilterFactories,
			PCGExFactories::PointFilters, true))
		{
			return false;
		}
	}

	if (!PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, true))
	{
		return false;
	};


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

		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->StartFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->StopFilterFactories, FacadePreloader);

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			Factory->RegisterBuffersDependencies(Context, FacadePreloader);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeRolling::Process);

		//PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (!Context->StartFilterFactories.IsEmpty())
		{
			StartFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!StartFilterManager->Init(Context, Context->StartFilterFactories)) { return false; }
		}

		if (!Context->StopFilterFactories.IsEmpty())
		{
			StopFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!StopFilterManager->Init(Context, Context->StopFilterFactories)) { return false; }
		}

		BlendOps = MakeShared<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>>();
		BlendOps->Reserve(Context->BlendingFactories.Num());

		if (!PCGExDataBlending::PrepareBlendOps(Context, PointDataFacade, Context->BlendingFactories, BlendOps))
		{
			return false;
		}

		MaxIndex = PointDataFacade->GetNum(PCGExData::ESource::In) - 1;
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();

		if (Settings->ToggleInitialValueMode == EPCGExRollingToggleInitialValue::FromPoint)
		{
			bRoll = StartFilterManager->Test(SourceIndex);
		}
		else
		{
			bRoll = Settings->bInitialToggle;
		}

		bDaisyChainProcessRange = true;
		StartParallelLoopForRange(PointDataFacade->GetNum());

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		if (PrimaryFilters->Test(Iteration)) { SourceIndex = Iteration; }

		const int32 TargetIndex = Settings->bReverseRolling ? MaxIndex - Iteration : Iteration;

		const bool bStart = StartFilterManager->Test(TargetIndex);

		if (Settings->Mode == EPCGExRollingMode::Toggle)
		{
			// Don't flip the switch if mode == Preserve
			// if(Settings->ToggleInitialValueMode == EPCGExRollingToggleInitialValue::ConstantPreserve)
			if (bStart) { bRoll = !bRoll; }
		}
		else
		{
			if (StopFilterManager->Test(TargetIndex)) { bRoll = false; }
			else if (bStart) { bRoll = true; }
		}

		if (!bRoll) { return; }

		if (SourceIndex == TargetIndex) { return; } // First or last point

		if (SourceIndex >= 0)
		{
			TArray<FPCGPoint>& PathPoints = PointDataFacade->GetMutablePoints();
			const TArray<TSharedPtr<FPCGExAttributeBlendOperation>>& BlendOpsRef = *BlendOps.Get();
			for (const TSharedPtr<FPCGExAttributeBlendOperation>& Op : BlendOpsRef)
			{
				// TODO : Compute alpha based on distance
				Op->Blend(SourceIndex, PathPoints[SourceIndex], TargetIndex, PathPoints[TargetIndex]);
			}
		}

		if (bStart) { SourceIndex = TargetIndex; }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

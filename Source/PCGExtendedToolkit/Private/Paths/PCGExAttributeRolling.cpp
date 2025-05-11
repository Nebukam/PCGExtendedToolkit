// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExAttributeRolling.h"

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"

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

	if (RangeControl == EPCGExRollingRangeControl::StartStop)
	{
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceStartConditionLabel, "...", Required, {})
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceStopConditionLabel, "...", Required, {})
	}
	else
	{
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceToggleConditionLabel, "...", Required, {})
	}

	if (ValueControl == EPCGExRollingValueControl::Pin)
	{
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourcePinConditionLabel, "...", Required, {})
	}

	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)

	PCGEX_FOREACH_FIELD_ATTRIBUTE_ROLL(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->RangeControl == EPCGExRollingRangeControl::StartStop)
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

	if (Settings->ValueControl == EPCGExRollingValueControl::Pin)
	{
		if (!PCGExFactories::GetInputFactories<UPCGExFilterFactoryData>(
			Context, PCGExPointFilter::SourcePinConditionLabel, Context->PinFilterFactories,
			PCGExFactories::PointFilters, true))
		{
			return false;
		}
	}

	PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

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

		{
			// Initialize attributes before blend ops so they can be used during the rolling
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_ATTRIBUTE_ROLL(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bReverseRolling) { SourceOffset = 1; }

		if (!Context->PinFilterFactories.IsEmpty())
		{
			PinFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!PinFilterManager->Init(Context, Context->PinFilterFactories)) { return false; }
		}

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

		FirstIndex = Settings->bReverseRolling ? MaxIndex : 0;
		RangeIndex += Settings->RangeIndexOffset;

		if (Settings->InitialValueMode == EPCGExRollingToggleInitialValue::FromPoint)
		{
			bRoll = StartFilterManager->Test(FirstIndex);
		}
		else
		{
			bRoll = Settings->bInitialValue;
		}

		SourceIndex = bRoll ? FirstIndex : -1;

		bDaisyChainProcessRange = true;
		StartParallelLoopForRange(PointDataFacade->GetNum());

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		const int32 TargetIndex = Settings->bReverseRolling ? MaxIndex - Iteration : Iteration;

		if (Settings->ValueControl == EPCGExRollingValueControl::Pin)
		{
			if (PinFilterManager->Test(Iteration)) { SourceIndex = Iteration; }
		}
		else if (Settings->ValueControl == EPCGExRollingValueControl::Previous)
		{
			SourceIndex = Iteration + SourceOffset;
			if (SourceIndex < 0 || SourceIndex > MaxIndex)
			{
				// TODO : Handle closed paths?
				SourceIndex = -1;
			}
		}

		const bool bPreviousRoll = bRoll;
		const bool bStart = StartFilterManager->Test(TargetIndex);
		bool bStop = false;

		if (Settings->RangeControl == EPCGExRollingRangeControl::Toggle)
		{
			if (bStart)
			{
				bRoll = !bRoll;
				bStop = !bRoll;
			}
		}
		else
		{
			if (StopFilterManager->Test(TargetIndex))
			{
				bRoll = false;
				bStop = true;
			}
			else if (bStart)
			{
				bRoll = true;
			}
		}

		if (bPreviousRoll != bRoll || TargetIndex == FirstIndex)
		{
			PCGEX_OUTPUT_VALUE(RangePole, TargetIndex, true)

			if (bRoll)
			{
				// New range started
				RangeIndex++;
				InternalRangeIndex = -1;

				PCGEX_OUTPUT_VALUE(RangeStart, TargetIndex, true)

				if (Settings->ValueControl == EPCGExRollingValueControl::RangeStart)
				{
					SourceIndex = TargetIndex;
				}
			}
			else
			{
				// Current range stopped

				PCGEX_OUTPUT_VALUE(RangeStop, TargetIndex, true)
			}
		}

		InternalRangeIndex++;

		PCGEX_OUTPUT_VALUE(RangeIndex, TargetIndex, RangeIndex)
		PCGEX_OUTPUT_VALUE(IndexInsideRange, TargetIndex, InternalRangeIndex)
		PCGEX_OUTPUT_VALUE(IsInsideRange, TargetIndex, bRoll)

		if (!bRoll && !Settings->bBlendOutsideRange)
		{
			if (bStop && Settings->bBlendStopElement)
			{
				// Skip that one roll
			}
			else { return; }
		}

		if (SourceIndex != -1)
		{
			TArray<FPCGPoint>& PathPoints = PointDataFacade->GetMutablePoints();
			const TArray<TSharedPtr<FPCGExAttributeBlendOperation>>& BlendOpsRef = *BlendOps.Get();
			for (const TSharedPtr<FPCGExAttributeBlendOperation>& Op : BlendOpsRef)
			{
				// TODO : Compute alpha based on distance
				Op->Blend(SourceIndex, PathPoints[SourceIndex], TargetIndex, PathPoints[TargetIndex]);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

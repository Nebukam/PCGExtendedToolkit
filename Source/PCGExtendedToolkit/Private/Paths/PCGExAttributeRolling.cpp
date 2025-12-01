// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExAttributeRolling.h"

#include "PCGExLabels.h"
#include "PCGExMT.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"

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
		PCGEX_PIN_FILTERS(PCGExPointFilter::SourceStartConditionLabel, "...", Required)
		PCGEX_PIN_FILTERS(PCGExPointFilter::SourceStopConditionLabel, "...", Required)
	}
	else
	{
		PCGEX_PIN_FILTERS(PCGExPointFilter::SourceToggleConditionLabel, "...", Normal)
	}

	if (ValueControl == EPCGExRollingValueControl::Pin)
	{
		PCGEX_PIN_FILTERS(PCGExPointFilter::SourcePinConditionLabel, "...", Required)
	}

	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal);

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

PCGExData::EIOInit UPCGExAttributeRollingSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)

	PCGEX_FOREACH_FIELD_ATTRIBUTE_ROLL(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->RangeControl == EPCGExRollingRangeControl::StartStop)
	{
		if (!PCGExFactories::GetInputFactories<UPCGExPointFilterFactoryData>(
			Context, PCGExPointFilter::SourceStartConditionLabel, Context->StartFilterFactories,
			PCGExFactories::PointFilters))
		{
			return false;
		}

		if (!PCGExFactories::GetInputFactories<UPCGExPointFilterFactoryData>(
			Context, PCGExPointFilter::SourceStopConditionLabel, Context->StopFilterFactories,
			PCGExFactories::PointFilters))
		{
			return false;
		}
	}
	else
	{
		PCGExFactories::GetInputFactories<UPCGExPointFilterFactoryData>(
			Context, PCGExPointFilter::SourceToggleConditionLabel, Context->StartFilterFactories,
			PCGExFactories::PointFilters, false);
	}

	if (Settings->ValueControl == EPCGExRollingValueControl::Pin)
	{
		if (!PCGExFactories::GetInputFactories<UPCGExPointFilterFactoryData>(
			Context, PCGExPointFilter::SourcePinConditionLabel, Context->PinFilterFactories,
			PCGExFactories::PointFilters))
		{
			return false;
		}
	}

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	return true;
}


bool FPCGExAttributeRollingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRollingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				Entry->InitializeOutput(PCGExData::EIOInit::Duplicate);
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bPrefetchData = !Context->BlendingFactories.IsEmpty();
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to roll over."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainBatch->Output();
	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExAttributeRolling
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor<FPCGExAttributeRollingContext, UPCGExAttributeRollingSettings>::RegisterBuffersDependencies(FacadePreloader);

		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->StartFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->StopFilterFactories, FacadePreloader);
		PCGExDataBlending::RegisterBuffersDependencies(Context, FacadePreloader, Context->BlendingFactories);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeRolling::Process);

		//PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

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

		if (!Context->BlendingFactories.IsEmpty())
		{
			BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>();
			BlendOpsManager->SetTargetFacade(PointDataFacade);
			BlendOpsManager->SetSources(PointDataFacade, PCGExData::EIOSide::Out);
			if (!BlendOpsManager->Init(Context, Context->BlendingFactories))
			{
				return false;
			}
		}

		MaxIndex = PointDataFacade->GetNum(PCGExData::EIOSide::In) - 1;

		FirstIndex = Settings->bReverseRolling ? MaxIndex : 0;
		RangeIndex += Settings->RangeIndexOffset;

		if (Settings->InitialValueMode == EPCGExRollingToggleInitialValue::FromPoint)
		{
			if (!StartFilterManager)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Initial toggle from point requires valid filters."));
				return false;
			}

			bRoll = StartFilterManager->Test(FirstIndex);
		}
		else
		{
			bRoll = Settings->bInitialValue;
		}

		SourceIndex = bRoll ? FirstIndex : -1;

		bForceSingleThreadedProcessRange = true;
		StartParallelLoopForRange(PointDataFacade->GetNum());

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 TargetIndex = Settings->bReverseRolling ? MaxIndex - Index : Index;

			if (Settings->ValueControl == EPCGExRollingValueControl::Pin)
			{
				if (PinFilterManager->Test(Index)) { SourceIndex = Index; }
			}
			else if (Settings->ValueControl == EPCGExRollingValueControl::Previous)
			{
				SourceIndex = Index + SourceOffset;
				if (SourceIndex < 0 || SourceIndex > MaxIndex)
				{
					// TODO : Handle closed paths?
					SourceIndex = -1;
				}
			}

			const bool bPreviousRoll = bRoll;
			const bool bStart = StartFilterManager ? StartFilterManager->Test(TargetIndex) : false;
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
					// Don't skip that one roll
				}
				else
				{
					continue;
				}
			}

			if (SourceIndex != -1 && BlendOpsManager)
			{
				BlendOpsManager->BlendAutoWeight(SourceIndex, TargetIndex);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (BlendOpsManager) { BlendOpsManager->Cleanup(Context); }
		PointDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

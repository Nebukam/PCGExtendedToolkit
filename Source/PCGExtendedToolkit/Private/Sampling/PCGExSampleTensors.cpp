// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleTensors.h"
#include "Transform/Tensors/PCGExTensor.h"

#define PCGEX_FOREACH_FIELD_TENSOR(MACRO)\
MACRO(Success, bool, false)\
MACRO(Transform, FTransform, FTransform::Identity)

#define LOCTEXT_NAMESPACE "PCGExSampleTensorsElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

UPCGExSampleTensorsSettings::UPCGExSampleTensorsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExSampleTensorsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExTensor::SourceTensorsLabel, "Tensors to sample", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleTensorsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

void FPCGExSampleTensorsContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleTensors)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
}

PCGEX_INITIALIZE_ELEMENT(SampleTensors)

bool FPCGExSampleTensorsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleTensors)

	// TODO GetFactories
	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGExTensor::SourceTensorsLabel);

	if (!Targets.IsEmpty())
	{
	}

	PCGEX_FOREACH_FIELD_TENSOR(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleTensorsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleTensorsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleTensors)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleTensors::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleTensors::FProcessor>>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExSampleTensors
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleTensors::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_TENSOR(PCGEX_OUTPUT_INIT)
		}
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point, const double InDepth)
	{
		SampleState[Index] = false;

		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
	}

	void FProcessor::ProcessSinglePoint(int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, Point, 0); }
			return;
		}

		//SampleState[Index] = Stats.IsValid();
		//PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
		//PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)

		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

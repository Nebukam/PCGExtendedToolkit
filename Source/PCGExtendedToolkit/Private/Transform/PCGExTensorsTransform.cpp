// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTensorsTransform.h"

#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExTensorsTransformElement"
#define PCGEX_NAMESPACE TensorsTransform

TArray<FPCGPinProperties> UPCGExTensorsTransformSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExTensor::SourceTensorsLabel, "Tensors", Required, {})
	PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceStopConditionLabel, "Transformed points will be tested against those filters, and transform will stop at first fail. Only a small subset of PCGEx are supported.", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(TensorsTransform)

bool FPCGExTensorsTransformElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TensorsTransform)

	if (!PCGExFactories::GetInputFactories(InContext, PCGExTensor::SourceTensorsLabel, Context->TensorFactories, {PCGExFactories::EType::Tensor}, true)) { return false; }
	if (Context->TensorFactories.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing tensors."));
		return false;
	}

	PCGEX_FOREACH_FIELD_TRTENSOR(PCGEX_OUTPUT_VALIDATE_NAME)

	GetInputFactories(Context, PCGExPointFilter::SourceStopConditionLabel, Context->StopFilterFactories, PCGExFactories::PointFilters, false);
	PCGExPointFilter::PruneForDirectEvaluation(Context, Context->StopFilterFactories);

	return true;
}

bool FPCGExTensorsTransformElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTensorsTransformElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TensorsTransform)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExTensorsTransform::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExTensorsTransform::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExTensorsTransform
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExTensorsTransform::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		if (!Context->StopFilterFactories.IsEmpty())
		{
			StopFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!StopFilters->Init(Context, Context->StopFilterFactories)) { StopFilters.Reset(); }
		}

		TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>(Settings->TensorHandlerDetails);
		if (!TensorsHandler->Init(Context, Context->TensorFactories, PointDataFacade)) { return false; }

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_TRTENSOR(PCGEX_OUTPUT_INIT)
		}

		RemainingIterations = Settings->Iterations;
		Metrics.SetNum(PointDataFacade->GetNum());
		Pings.Init(0, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::EIOSide::Out, 64);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::TensorTransform::ProcessPoints);

		if (bIteratedOnce) { return; }

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange(false);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			bool bSuccess = false;
			const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(Index, OutTransforms[Index], bSuccess);
			PointFilterCache[Index] = bSuccess;

			if (!bSuccess)
			{
				// TODO : Gracefully stopped
				// TODO : Max iterations not reached
				continue;
			}

			if (StopFilters)
			{
				PCGExData::FProxyPoint ProxyPoint = PCGExData::FProxyPoint(PCGExData::FMutablePoint(PointDataFacade->GetOutPoint(Index))); // Oooof
				if (StopFilters->Test(ProxyPoint))
				{
					PointFilterCache[Index] = false;
					if (Settings->StopConditionHandling == EPCGExTensorStopConditionHandling::Exclude)
					{
						// TODO : Not gracefully stopped
						// TODO : Max iterations not reached
						continue;
					}
				}
			}

			Metrics[Index].Add(OutTransforms[Index].GetLocation());
			Pings[Index] += Sample.Effectors;

			if (Settings->bTransformRotation)
			{
				if (Settings->Rotation == EPCGExTensorTransformMode::Absolute)
				{
					OutTransforms[Index].SetRotation(Sample.Rotation);
				}
				else if (Settings->Rotation == EPCGExTensorTransformMode::Relative)
				{
					OutTransforms[Index].SetRotation(Sample.Rotation * OutTransforms[Index].GetRotation());
				}
				else if (Settings->Rotation == EPCGExTensorTransformMode::Align)
				{
					OutTransforms[Index].SetRotation(PCGExMath::MakeDirection(Settings->AlignAxis, Sample.DirectionAndSize.GetSafeNormal() * -1, OutTransforms[Index].GetRotation().GetUpVector()));
				}
			}

			if (Settings->bTransformPosition)
			{
				OutTransforms[Index].SetLocation(OutTransforms[Index].GetLocation() + Sample.DirectionAndSize);
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		bIteratedOnce = true;
		RemainingIterations--;
		if (RemainingIterations > 0) { StartParallelLoopForPoints(PCGExData::EIOSide::Out, 32); }
		else { StartParallelLoopForRange(PointDataFacade->GetNum()); }
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExPaths::FPathMetrics& Metric = Metrics[Index];
			const int32 UpdateCount = Metric.Count;

			PCGEX_OUTPUT_VALUE(EffectorsPings, Index, Pings[Index])
			PCGEX_OUTPUT_VALUE(UpdateCount, Index, UpdateCount)
			PCGEX_OUTPUT_VALUE(TraveledDistance, Index, Metric.Length)
			PCGEX_OUTPUT_VALUE(GracefullyStopped, Index, UpdateCount < Settings->Iterations)
			PCGEX_OUTPUT_VALUE(MaxIterationsReached, Index, UpdateCount == Settings->Iterations)
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

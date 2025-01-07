// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTensorsTransform.h"

#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExTensorsTransformElement"
#define PCGEX_NAMESPACE TensorsTransform

TArray<FPCGPinProperties> UPCGExTensorsTransformSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExTensor::SourceTensorsLabel, "Tensors", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExTensorsTransformSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(TensorsTransform)

bool FPCGExTensorsTransformElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TensorsTransform)

	Context->TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>();
	if (!Context->TensorsHandler->Init(Context, PCGExTensor::SourceTensorsLabel))
	{
		return false;
	}

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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExTensorsTransform::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		RemainingIterations = Settings->Iterations;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		if (bIteratedOnce) { return; }
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if (!PointFilterCache[Index]) { return; }

		bool bSuccess = false;
		const PCGExTensor::FTensorSample Sample = Context->TensorsHandler->SampleAtPosition(Point.Transform.GetLocation(), bSuccess);
		PointFilterCache[Index] = bSuccess;

		if (!bSuccess) { return; }

		//const FTransform Composite = Point.Transform * Sample.Transform;

		if (Settings->bTransformRotation)
		{
			if (Settings->Rotation == EPCGExTensorTransformMode::Absolute)
			{
				Point.Transform.SetRotation(Sample.Rotation);
			}
			else if (Settings->Rotation == EPCGExTensorTransformMode::Relative)
			{
				Point.Transform.SetRotation(Point.Transform.GetRotation() * Sample.Rotation);
			}
			else if (Settings->Rotation == EPCGExTensorTransformMode::Align)
			{
				Point.Transform.SetRotation(PCGExMath::MakeDirection(Settings->AlignAxis, Sample.DirectionAndSize.GetSafeNormal() * -1, Point.Transform.GetRotation().GetUpVector()));
			}
		}

		if (Settings->bTransformPosition)
		{
			Point.Transform.SetLocation(Point.Transform.GetLocation() + Sample.DirectionAndSize);
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		bIteratedOnce = true;
		RemainingIterations--;
		if (RemainingIterations > 0) { StartParallelLoopForPoints(); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

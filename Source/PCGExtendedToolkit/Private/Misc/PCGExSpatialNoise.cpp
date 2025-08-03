// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExSpatialNoise.h"

#include "Data/PCGSplineData.h"

#define LOCTEXT_NAMESPACE "PCGExSpatialNoiseElement"
#define PCGEX_NAMESPACE SpatialNoise

#if WITH_EDITOR
FString UPCGExSpatialNoiseSettings::GetDisplayName() const
{
	return PCGEx::GetSelectorDisplayName(OutputTarget);
}
#endif

PCGEX_INITIALIZE_ELEMENT(SpatialNoise)

bool FPCGExSpatialNoiseElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SpatialNoise)

	return true;
}

bool FPCGExSpatialNoiseElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSpatialNoiseElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SpatialNoise)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSpatialNoise::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSpatialNoise::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExSpatialNoise
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSpatialNoise::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(PointDataFacade->GetOut(), Settings->OutputTarget, false);
		if (!OutAccessor) { return false; }

		StartParallelLoopForPoints();
		
		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SpatialNoise::ProcessPoints);

		PCGEX_SCOPE_LOOP(Index)
		{
			
		}
	}

	void FProcessor::CompleteWork()
	{
		//PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

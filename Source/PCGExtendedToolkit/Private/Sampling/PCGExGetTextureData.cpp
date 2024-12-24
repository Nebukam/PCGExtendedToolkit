// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExGetTextureData.h"

#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsSettings.h"


#define LOCTEXT_NAMESPACE "PCGExGetTextureDataElement"
#define PCGEX_NAMESPACE GetTextureData

UPCGExGetTextureDataSettings::UPCGExGetTextureDataSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

TArray<FPCGPinProperties> UPCGExGetTextureDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	//if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required, {}) }
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExGetTextureDataSettings::OutputPinProperties() const
{
	return Super::OutputPinProperties();
}

PCGExData::EIOInit UPCGExGetTextureDataSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(GetTextureData)

bool FPCGExGetTextureDataElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(GetTextureData)

	return true;
}

bool FPCGExGetTextureDataElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGetTextureDataElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(GetTextureData)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExGetTextureData::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExGetTextureData::FProcessor>>& NewBatch)
			{
				//if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to sample."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExGetTextureData
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExGetTextureData::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		
	}

	void FProcessor::CompleteWork()
	{
		
	}

	void FProcessor::Write()
	{
		
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

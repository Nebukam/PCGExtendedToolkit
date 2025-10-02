﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSockets.h"

#include "PCGComponent.h"
#include "PCGExHelpers.h"
#include "AssetStaging/PCGExSocketStaging.h"
#include "AssetStaging/PCGExStaging.h"
#include "Data/PCGExDataTag.h"
#include "Metadata/PCGObjectPropertyOverride.h"


#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSocketsElement"
#define PCGEX_NAMESPACE BuildCustomGraph


PCGEX_INITIALIZE_ELEMENT(SampleSockets)

TArray<FPCGPinProperties> UPCGExSampleSocketsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExStaging::OutputSocketLabel, "Socket points.", Normal)
	return PinProperties;
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleSockets)

void FPCGExSampleSocketsContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	if (StaticMeshLoader) { StaticMeshLoader->AddExtraStructReferencedObjects(Collector); }
	if (StaticMesh) { Collector.AddReferencedObject(StaticMesh); }

	FPCGExPointsProcessorContext::AddExtraStructReferencedObjects(Collector);
}

bool FPCGExSampleSocketsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSockets)

	PCGEX_FWD(OutputSocketDetails)
	if (!Context->OutputSocketDetails.Init(Context)) { return false; }

	if (Settings->AssetType == EPCGExInputValueType::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->AssetPathAttributeName)

		TArray<FName> Names = {Settings->AssetPathAttributeName};
		Context->StaticMeshLoader = MakeShared<PCGEx::TAssetLoader<UStaticMesh>>(Context, Context->MainPoints.ToSharedRef(), Names);
	}
	else
	{
		Context->StaticMesh = PCGExHelpers::LoadBlocking_AnyThread(Settings->StaticMesh);
		if (!Context->StaticMesh)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Static mesh could not be loaded."));
			return false;
		}
	}

	Context->SocketsCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->SocketsCollection->OutputPin = PCGExStaging::OutputSocketLabel;

	return true;
}

bool FPCGExSampleSocketsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSocketsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleSockets)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Context->StaticMesh)
		{
			Context->SetState(PCGExCommon::State_WaitingOnAsyncWork);
		}
		else
		{
			Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);

			if (!Context->StaticMeshLoader->Start(Context->GetAsyncManager()))
			{
				return Context->CancelExecution(TEXT("Failed to find any asset to load."));
			}

			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
	{
		if (Context->StaticMeshLoader && Context->StaticMeshLoader->IsEmpty())
		{
			return Context->CancelExecution(TEXT("Failed to load any assets."));
		}

		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to write tangents to."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->SocketsCollection->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleSockets
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->AssetType == EPCGExInputValueType::Attribute)
		{
			AssetPathReader = PointDataFacade->GetBroadcaster<FSoftObjectPath>(Settings->AssetPathAttributeName, true);
			if (!AssetPathReader) { return false; }
		}

		SocketHelper = MakeShared<PCGExStaging::FSocketHelper>(&Context->OutputSocketDetails, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleSockets::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			const TObjectPtr<UStaticMesh>* SM = AssetPathReader ? Context->StaticMeshLoader->GetAsset(AssetPathReader->Read(Index)) : &Context->StaticMesh;

			if (!SM) { continue; }

			SocketHelper->Add(Index, *SM);
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		SocketHelper->Compile(AsyncManager, PointDataFacade, Context->SocketsCollection);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExSampleSockets.h"

#include "Engine/StaticMesh.h"
#include "PCGComponent.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExAssetLoader.h"
#include "Helpers/PCGExSocketHelpers.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "Metadata/PCGObjectPropertyOverride.h"

#include "Paths/PCGExPath.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSocketsElement"
#define PCGEX_NAMESPACE BuildCustomGraph


PCGEX_INITIALIZE_ELEMENT(SampleSockets)

TArray<FPCGPinProperties> UPCGExSampleSocketsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExStaging::Labels::OutputSocketLabel, "Socket points.", Normal)
	return PinProperties;
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleSockets)

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
		Context->StaticMeshLoader = MakeShared<PCGEx::TAssetLoader<UStaticMesh>>(Context, Context->MainPoints, Names);
	}
	else
	{
		PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->StaticMesh, Context);
		Context->StaticMesh = Settings->StaticMesh.Get();
		if (!Context->StaticMesh)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Static mesh could not be loaded."));
			return false;
		}
	}

	Context->SocketsCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->SocketsCollection->OutputPin = PCGExStaging::Labels::OutputSocketLabel;

	return true;
}

bool FPCGExSampleSocketsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSocketsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleSockets)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Context->StaticMesh)
		{
			Context->SetState(PCGExCommon::States::State_WaitingOnAsyncWork);
		}
		else
		{
			Context->SetState(PCGExCommon::States::State_WaitingOnAsyncWork);

			if (!Context->StaticMeshLoader->Start(Context->GetTaskManager()))
			{
				return Context->CancelExecution(TEXT("Failed to find any asset to load."));
			}

			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_WaitingOnAsyncWork)
	{
		if (Context->StaticMeshLoader && Context->StaticMeshLoader->IsEmpty())
		{
			return Context->CancelExecution(TEXT("Failed to load any assets."));
		}

		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to write tangents to."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->SocketsCollection->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleSockets
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Settings->AssetType == EPCGExInputValueType::Attribute)
		{
			Keys = Context->StaticMeshLoader->GetKeys(PointDataFacade->Source->IOIndex);
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

		const TArray<PCGExValueHash>& KeysRef = Keys ? *Keys.Get() : TArray<PCGExValueHash>{};

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			const TObjectPtr<UStaticMesh>* SM = Keys ? Context->StaticMeshLoader->GetAsset(KeysRef[Index]) : &Context->StaticMesh;
			if (!SM) { continue; }
			SocketHelper->Add(Index, *SM);
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		SocketHelper->Compile(TaskManager, PointDataFacade, Context->SocketsCollection);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

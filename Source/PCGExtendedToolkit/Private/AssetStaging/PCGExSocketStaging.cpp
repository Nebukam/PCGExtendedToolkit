// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExSocketStaging.h"

#include "PCGExRandom.h"
#include "PCGExScopedContainers.h"


#define LOCTEXT_NAMESPACE "PCGExSocketStagingElement"
#define PCGEX_NAMESPACE SocketStaging

PCGEX_INITIALIZE_ELEMENT(SocketStaging)
PCGEX_ELEMENT_BATCH_POINT_IMPL(SocketStaging)

TArray<FPCGPinProperties> UPCGExSocketStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExSocketStaging::SourceStagingMap, "Collection map information from, or merged from, Staging nodes.", Required, {})
	return PinProperties;
}

bool FPCGExSocketStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SocketStaging)

	Context->CollectionPickDatasetUnpacker = MakeShared<PCGExStaging::TPickUnpacker<>>();
	Context->CollectionPickDatasetUnpacker->UnpackPin(InContext, PCGExSocketStaging::SourceStagingMap);

	if (!Context->CollectionPickDatasetUnpacker->HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not rebuild a valid asset mapping from the provided map."));
		return false;
	}

	return true;
}

bool FPCGExSocketStagingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSocketStagingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SocketStaging)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();
	return Context->TryComplete();
}

bool FPCGExSocketStagingElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	// Loading collection and/or creating one from attributes
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExSocketStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSocketStaging::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// TODO : First, discovery & building per-hash map; ignoring material pick
		// Also compute the total number of points
		// TODO : Second, 

		EntryHashGetter = PointDataFacade->GetReadable<int64>(PCGExStaging::Tag_EntryIdx, PCGExData::EIOSide::In, true);


		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SocketStaging::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TMap<uint64, const FPCGExAssetCollectionEntry*> TempEntryMap;
		TMap<uint64, const FPCGExAssetCollectionEntry*> TempEntryCount;

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }
		}

		{
			FWriteScopeLock WriteScopeLock(EntryMapLock);
		}
	}

	void FProcessor::CompleteWork()
	{
		// Full map and counts is accounted for
		// We can now find which socket is relevant for the output

		// TODO : Use SocketCount
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
	}

}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

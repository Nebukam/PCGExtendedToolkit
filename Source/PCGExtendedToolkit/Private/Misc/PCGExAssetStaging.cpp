// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAssetStaging.h"

#define LOCTEXT_NAMESPACE "PCGExAssetStagingElement"
#define PCGEX_NAMESPACE AssetStaging

PCGExData::EInit UPCGExAssetStagingSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(AssetStaging)

FPCGExAssetStagingContext::~FPCGExAssetStagingContext()
{
	PCGEX_TERMINATE_ASYNC
}

void FPCGExAssetStagingContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	// TODO : Load staging data from collection
	// Feed all the data to the context
	// if failed, then manually set bLoadError = true;
	PCGEX_SETTINGS_LOCAL(AssetStaging)

	MainCollection = Settings->MainCollection.LoadSynchronous();

	if (!MainCollection)
	{
		bAssetLoadError = true;
		return;
	}

	//MainCollection->BuildCache(TODO);
	//MainCollection->AppendStaging(MainCollection->GetStaging(), false);
}

bool FPCGExAssetStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	//PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)

	return true;
}

bool FPCGExAssetStagingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAssetStagingElement::Execute);

	PCGEX_CONTEXT(AssetStaging)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExAssetStaging
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAssetStaging::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(AssetStaging)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		NumPoints = PointIO->GetNum();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPrunePath.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExPrunePathElement"
#define PCGEX_NAMESPACE PrunePath

PCGExData::EInit UPCGExPrunePathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPrunePathSettings::GetPointFilterLabel() const { return FName("TriggerConditions"); }

PCGEX_INITIALIZE_ELEMENT(PrunePath)

FPCGExPrunePathContext::~FPCGExPrunePathContext()
{
	PCGEX_TERMINATE_ASYNC
}


bool FPCGExPrunePathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

	//PCGEX_FWD(bDoBlend)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExPrunePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPrunePathElement::Execute);

	PCGEX_CONTEXT(PrunePath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPrunePath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPrunePath::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPrunePath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPrunePath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PrunePath)

		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		bCurrentSwitch = Settings->bInitialSwitchValue;

		if (!Settings->bGenerateNewPaths) { CreateNewIO(); }

		bInlineProcessPoints = true;
		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		CachedIndex++;
		bool Trigger = LocalSettings->bInvertFilterValue ? !PointFilterCache[Index] : PointFilterCache[Index];

		if (LocalSettings->TriggerMode == EPCGExPathPruneTriggerMode::Switch)
		{
			if (Trigger) { bCurrentSwitch = !bCurrentSwitch; }
			Trigger = bCurrentSwitch;
		}

		if (Trigger)
		{
			if (LocalSettings->bGenerateNewPaths) { OutPoints = nullptr; }
			return;
		}
		
		if (!OutPoints) { CreateNewIO(); }

		FPCGPoint& NewPoint = OutPoints->Add_GetRef(Point);
		OutMetadata->InitializeOnSet(NewPoint.MetadataEntry);
	}

	void FProcessor::CreateNewIO()
	{
		// Create new point IO
		const PCGExData::FPointIO* NewIO = LocalTypedContext->MainPoints->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);

		OutPoints = &NewIO->GetOut()->GetMutablePoints();
		OutPoints->Reserve(PointIO->GetNum() - CachedIndex);

		OutMetadata = NewIO->GetOut()->Metadata;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

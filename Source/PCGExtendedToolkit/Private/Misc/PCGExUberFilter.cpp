// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExUberFilter"
#define PCGEX_NAMESPACE UberFilter

TArray<FPCGPinProperties> UPCGExUberFilterSettings::OutputPinProperties() const
{
	if (Mode == EPCGExUberFilterMode::Write) { return Super::OutputPinProperties(); }

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Points that passed the filters.", Required, {})
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Points that didn't pass the filters.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExUberFilterSettings::GetMainOutputInitMode() const { return Mode == EPCGExUberFilterMode::Write ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::NoOutput; }

FPCGExUberFilterContext::~FPCGExUberFilterContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Inside)
	PCGEX_DELETE(Outside)
}

PCGEX_INITIALIZE_ELEMENT(UberFilter)

FName UPCGExUberFilterSettings::GetMainOutputLabel() const
{
	// Ensure proper forward when node is disabled
	return Mode == EPCGExUberFilterMode::Partition ? PCGExPointFilter::OutputInsideFiltersLabel : Super::GetMainOutputLabel();
}

bool FPCGExUberFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	if (Settings->Mode == EPCGExUberFilterMode::Write)
	{
		PCGEX_VALIDATE_NAME(Settings->ResultAttributeName)
		return true;
	}

	Context->Inside = new PCGExData::FPointIOCollection(Context);
	Context->Outside = new PCGExData::FPointIOCollection(Context);

	Context->Inside->DefaultOutputLabel = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->DefaultOutputLabel = PCGExPointFilter::OutputOutsideFiltersLabel;

	if (Settings->bSwap)
	{
		Context->Inside->DefaultOutputLabel = PCGExPointFilter::OutputOutsideFiltersLabel;
		Context->Outside->DefaultOutputLabel = PCGExPointFilter::OutputInsideFiltersLabel;
	}

	return true;
}

bool FPCGExUberFilterElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->NumPairs = Context->MainPoints->Pairs.Num();

		if (Settings->Mode == EPCGExUberFilterMode::Partition)
		{
			PCGEX_SET_NUM_NULLPTR(Context->Inside->Pairs, Context->NumPairs)
			PCGEX_SET_NUM_NULLPTR(Context->Outside->Pairs, Context->NumPairs)
		}

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExUberFilter::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExUberFilter::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to filter."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Settings->Mode == EPCGExUberFilterMode::Write)
	{
		Context->MainPoints->OutputToContext();
	}
	else
	{
		Context->Inside->PruneNullEntries(true);
		Context->Outside->PruneNullEntries(true);

		Context->Inside->OutputToContext();
		Context->Outside->OutputToContext();
	}

	return Context->TryComplete();
}

namespace PCGExUberFilter
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExUberFilter::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(UberFilter)

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = TypedContext->bScopedAttributeGet;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			Results = PointDataFacade->GetWriter<bool>(Settings->ResultAttributeName, false, false, true);
		}
		else
		{
			PCGEX_SET_NUM_UNINITIALIZED(PointFilterCache, PointIO->GetNum())
		}

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
		if (!Results) { return; }
		Results->Values[Index] = LocalSettings->bSwap ? !PointFilterCache[Index] : PointFilterCache[Index];
	}

	PCGExData::FPointIO* FProcessor::CreateIO(PCGExData::FPointIOCollection* InCollection, const PCGExData::EInit InitMode) const
	{
		PCGExData::FPointIO* NewPointIO = new PCGExData::FPointIO(Context, PointIO);
		NewPointIO->DefaultOutputLabel = InCollection->DefaultOutputLabel;
		NewPointIO->InitializeOutput(InitMode);
		InCollection->Pairs[BatchIndex] = NewPointIO;
		return NewPointIO;
	}

	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterProcessor::CompleteWork);

		if (LocalSettings->Mode == EPCGExUberFilterMode::Write)
		{
			PointDataFacade->Write(AsyncManagerPtr, true);
			return;
		}

		const int32 NumPoints = PointIO->GetNum();
		TArray<int32> Indices;
		PCGEX_SET_NUM_UNINITIALIZED(Indices, NumPoints)

		for (int i = 0; i < NumPoints; ++i) { Indices[i] = PointFilterCache[i] ? NumInside++ : NumOutside++; }

		if (NumInside == 0 || NumOutside == 0)
		{
			if (NumInside == 0) { Outside = CreateIO(LocalTypedContext->Outside, PCGExData::EInit::Forward); }
			else { Inside = CreateIO(LocalTypedContext->Inside, PCGExData::EInit::Forward); }
			return;
		}

		const TArray<FPCGPoint>& OriginalPoints = PointIO->GetIn()->GetPoints();

		Inside = CreateIO(LocalTypedContext->Inside, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& InsidePoints = Inside->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(InsidePoints, NumInside)

		Outside = CreateIO(LocalTypedContext->Outside, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& OutsidePoints = Outside->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(OutsidePoints, NumOutside)

		for (int i = 0; i < NumPoints; ++i)
		{
			if (PointFilterCache[i]) { InsidePoints[Indices[i]] = OriginalPoints[i]; }
			else { OutsidePoints[Indices[i]] = OriginalPoints[i]; }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

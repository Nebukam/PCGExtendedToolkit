// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExUberFilter"
#define PCGEX_NAMESPACE UberFilter

TArray<FPCGPinProperties> UPCGExUberFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	PCGEX_PIN_POINTS(GetMainInputLabel(), "The point data to be processed.", Required, {})
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, GetPointFilterTooltip(), Required, {})

	return PinProperties;
}

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

bool FPCGExUberFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	if (!GetInputFactories(
		InContext, PCGExPointFilter::SourceFiltersLabel, Context->FilterFactories,
		PCGExFactories::PointFilters, true))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(FTEXT("Missing {0}."), FText::FromName(PCGExPointFilter::SourceFiltersLabel)));
		return false;
	}

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

		LocalTypedContext = TypedContext;

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		if (!InitPrimaryFilters(&TypedContext->FilterFactories)) { return false; }

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			Results = PointDataFacade->GetWriter<bool>(Settings->ResultAttributeName, false, false, true);
		}
		else
		{
			PCGEX_SET_NUM_UNINITIALIZED(PointFilterCache, PointIO->GetNum())
		}

		TestTaskGroup = AsyncManager->CreateGroup();
		TestTaskGroup->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { PointDataFacade->Fetch(StartIndex, Count); });

		if (Results)
		{
			TestTaskGroup->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx) { Results->Values[Index] = PrimaryFilters->Test(Index); },
				PointIO->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}
		else
		{
			TestTaskGroup->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx) { PointFilterCache[Index] = PrimaryFilters->Test(Index); },
				PointIO->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterProcessor::CompleteWork);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(UberFilter)

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			if (Settings->bSwap) { for (bool& Result : Results->Values) { Result = !Result; } }
			PointDataFacade->Write(AsyncManagerPtr, true);
			return;
		}

		const int32 NumPoints = PointIO->GetNum();
		TArray<int32> Indices;
		PCGEX_SET_NUM_UNINITIALIZED(Indices, NumPoints)

		for (int i = 0; i < NumPoints; i++)
		{
			if (PointFilterCache[i]) { Indices[i] = NumInside++; }
			else { Indices[i] = NumOutside++; }
		}

		if (NumInside == 0 || NumOutside == 0)
		{
			if (NumInside == 0) { Outside = LocalTypedContext->Outside->Emplace_GetRef(PointIO, PCGExData::EInit::Forward); }
			else { Inside = LocalTypedContext->Inside->Emplace_GetRef(PointIO, PCGExData::EInit::Forward); }
			return;
		}

		const TArray<FPCGPoint>& OriginalPoints = PointIO->GetIn()->GetPoints();

		Inside = LocalTypedContext->Inside->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
		Inside->InitializeOutput(PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& InsidePoints = Inside->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(InsidePoints, NumInside)

		Outside = LocalTypedContext->Outside->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& OutsidePoints = Outside->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(OutsidePoints, NumOutside)

		for (int i = 0; i < NumPoints; i++)
		{
			if (PointFilterCache[i]) { InsidePoints[Indices[i]] = OriginalPoints[i]; }
			else { OutsidePoints[Indices[i]] = OriginalPoints[i]; }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

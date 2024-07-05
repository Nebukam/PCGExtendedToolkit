// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExUberFilter"
#define PCGEX_NAMESPACE UberFilter

TArray<FPCGPinProperties> UPCGExUberFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Points that passed the filters.", Required, {})
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Points that didn't pass the filters.", Required, {})
	return PinProperties;
}

FName UPCGExUberFilterSettings::GetPointFilterLabel() const { return PCGExPointFilter::SourceFiltersLabel; }

bool UPCGExUberFilterSettings::RequiresPointFilters() const { return true; }

PCGExData::EInit UPCGExUberFilterSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExUberFilterContext::~FPCGExUberFilterContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Inside)
	PCGEX_DELETE(Outside)
}

PCGEX_INITIALIZE_ELEMENT(UberFilter)

bool FPCGExUberFilterElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)


	Context->Inside = new PCGExData::FPointIOCollection();
	Context->Outside = new PCGExData::FPointIOCollection();

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

	Context->Inside->OutputTo(Context);
	Context->Outside->OutputTo(Context);

	return Context->TryComplete();
}

namespace PCGExUberFilter
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(UberFilter)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		for (const bool& Result : PointFilterCache)
		{
			if (Result) { NumInside++; }
			else { NumOutside++; }
		}

		InCollection = TypedContext->Inside;
		OutCollection = TypedContext->Outside;

		if (NumInside == 0 || NumOutside == 0)
		{
			PCGExData::FPointIOCollection* OutputCollection;

			if (NumInside == 0) { OutputCollection = OutCollection; }
			else { OutputCollection = InCollection; }

			OutputCollection->Emplace_GetRef(PointIO, PCGExData::EInit::Forward);

			InCollection = nullptr;
			OutCollection = nullptr;

			return true;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (!InCollection || !OutCollection) { return; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(UberFilter)

		const TArray<FPCGPoint>& OriginalPoints = PointIO->GetIn()->GetPoints();

		TArray<FPCGPoint>& InsidePoints = InCollection->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput)->GetOut()->GetMutablePoints();
		TArray<FPCGPoint>& OutsidePoints = OutCollection->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput)->GetOut()->GetMutablePoints();

		InsidePoints.Reserve(NumInside);
		OutsidePoints.Reserve(NumOutside);

		for (int i = 0; i < PointFilterCache.Num(); i++)
		{
			if (PointFilterCache[i]) { InsidePoints.Emplace(OriginalPoints[i]); }
			else { OutsidePoints.Emplace(OriginalPoints[i]); }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

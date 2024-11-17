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

PCGExData::EIOInit UPCGExUberFilterSettings::GetMainOutputInitMode() const { return Mode == EPCGExUberFilterMode::Write ? PCGExData::EIOInit::DuplicateInput : PCGExData::EIOInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(UberFilter)

FName UPCGExUberFilterSettings::GetMainOutputPin() const
{
	// Ensure proper forward when node is disabled
	return Mode == EPCGExUberFilterMode::Partition ? PCGExPointFilter::OutputInsideFiltersLabel : Super::GetMainOutputPin();
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

	Context->Inside = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Outside = MakeShared<PCGExData::FPointIOCollection>(Context);

	Context->Inside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;

	if (Settings->bSwap)
	{
		Context->Inside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;
		Context->Outside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	}

	return true;
}

bool FPCGExUberFilterElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->NumPairs = Context->MainPoints->Pairs.Num();

		if (Settings->Mode == EPCGExUberFilterMode::Partition)
		{
			Context->Inside->Pairs.Init(nullptr, Context->NumPairs);
			Context->Outside->Pairs.Init(nullptr, Context->NumPairs);
		}

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExUberFilter::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExUberFilter::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to filter."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->Mode == EPCGExUberFilterMode::Write)
	{
		Context->MainPoints->StageOutputs();
	}
	else
	{
		Context->Inside->PruneNullEntries(true);
		Context->Outside->PruneNullEntries(true);

		Context->Inside->StageOutputs();
		Context->Outside->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExUberFilter
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExUberFilter::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			Results = PointDataFacade->GetWritable<bool>(Settings->ResultAttributeName, false, true, PCGExData::EBufferInit::New);
		}
		else
		{
			PCGEx::InitArray(PointFilterCache, PointDataFacade->GetNum());
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
		int8 bPass = PointFilterCache[Index];

		if (bPass) { FPlatformAtomics::InterlockedAdd(&NumInside, 1); }
		else { FPlatformAtomics::InterlockedAdd(&NumOutside, 1); }

		if (!Results) { return; }
		Results->GetMutable(Index) = bPass ? !Settings->bSwap : Settings->bSwap;
	}

	TSharedPtr<PCGExData::FPointIO> FProcessor::CreateIO(const TSharedRef<PCGExData::FPointIOCollection>& InCollection, const PCGExData::EIOInit InitMode) const
	{
		TSharedPtr<PCGExData::FPointIO> NewPointIO = PCGExData::NewPointIO(PointDataFacade->Source, InCollection->OutputPin);
		NewPointIO->InitializeOutput(InitMode);
		InCollection->Pairs[BatchIndex] = NewPointIO;
		return NewPointIO;
	}

	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterProcessor::CompleteWork);

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			const bool bHasAnyPass = Settings->bSwap ? NumOutside != 0 : NumInside != 0;
			const bool bAllPass = Settings->bSwap ? NumOutside == PointDataFacade->GetNum() : NumInside == PointDataFacade->GetNum();
			if (bHasAnyPass && Settings->bTagIfAnyPointPassed) { PointDataFacade->Source->Tags->Add(Settings->HasAnyPointPassedTag); }
			if (bAllPass && Settings->bTagIfAllPointsPassed) { PointDataFacade->Source->Tags->Add(Settings->AllPointsPassedTag); }
			if (!bHasAnyPass && Settings->bTagIfNoPointPassed) { PointDataFacade->Source->Tags->Add(Settings->NoPointPassedTag); }


			PointDataFacade->Write(AsyncManager);
			return;
		}

		if (NumInside == 0 || NumOutside == 0)
		{
			if (NumInside == 0)
			{
				Outside = CreateIO(Context->Outside.ToSharedRef(), PCGExData::EIOInit::Forward);
				if (Settings->bTagIfNoPointPassed) { Outside->Tags->Add(Settings->NoPointPassedTag); }
			}
			else
			{
				Inside = CreateIO(Context->Inside.ToSharedRef(), PCGExData::EIOInit::Forward);
				if (Settings->bTagIfAnyPointPassed) { Inside->Tags->Add(Settings->HasAnyPointPassedTag); }
				if (Settings->bTagIfAllPointsPassed) { Inside->Tags->Add(Settings->AllPointsPassedTag); }
			}
			return;
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		TArray<int32> Indices;
		PCGEx::InitArray(Indices, NumPoints);

		NumInside = NumOutside = 0;
		for (int i = 0; i < NumPoints; i++) { Indices[i] = PointFilterCache[i] ? NumInside++ : NumOutside++; }

		const TArray<FPCGPoint>& OriginalPoints = PointDataFacade->GetIn()->GetPoints();

		Inside = CreateIO(Context->Inside.ToSharedRef(), PCGExData::EIOInit::NewOutput);
		TArray<FPCGPoint>& InsidePoints = Inside->GetOut()->GetMutablePoints();
		PCGEx::InitArray(InsidePoints, NumInside);

		Outside = CreateIO(Context->Outside.ToSharedRef(), PCGExData::EIOInit::NewOutput);
		TArray<FPCGPoint>& OutsidePoints = Outside->GetOut()->GetMutablePoints();
		PCGEx::InitArray(OutsidePoints, NumOutside);

		if (Settings->bTagIfAnyPointPassed) { Inside->Tags->Add(Settings->HasAnyPointPassedTag); }

		for (int i = 0; i < NumPoints; i++)
		{
			if (PointFilterCache[i]) { InsidePoints[Indices[i]] = OriginalPoints[i]; }
			else { OutsidePoints[Indices[i]] = OriginalPoints[i]; }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

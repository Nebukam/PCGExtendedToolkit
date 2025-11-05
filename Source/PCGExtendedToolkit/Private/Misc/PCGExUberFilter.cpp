// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"
#include "Misc/Pickers/PCGExPicker.h"
#include "Misc/Pickers/PCGExPickerFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExUberFilter"
#define PCGEX_NAMESPACE UberFilter

#if WITH_EDITOR
void UPCGExUberFilterSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (!ResultAttributeName_DEPRECATED.IsNone())
	{
		ResultDetails.ResultAttributeName = ResultAttributeName_DEPRECATED;
		ResultAttributeName_DEPRECATED = NAME_None;
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

bool UPCGExUberFilterSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExPicker::SourcePickersLabel) { return InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

bool UPCGExUberFilterSettings::OutputPinsCanBeDeactivated() const
{
	return Mode != EPCGExUberFilterMode::Write;
}

TArray<FPCGPinProperties> UPCGExUberFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExPicker::SourcePickersLabel, "A precise selection of point that will be tested, as opposed to all of them.", Normal, FPCGExDataTypeInfoPicker::AsId())
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUberFilterSettings::OutputPinProperties() const
{
	if (Mode == EPCGExUberFilterMode::Write) { return Super::OutputPinProperties(); }

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Points that passed the filters.", Required)
	if (bOutputDiscardedElements) { PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Points that didn't pass the filters.", Required) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(UberFilter)
PCGEX_ELEMENT_BATCH_POINT_IMPL(UberFilter)

FName UPCGExUberFilterSettings::GetMainOutputPin() const
{
	// Ensure proper forward when node is disabled
	return Mode == EPCGExUberFilterMode::Partition ? PCGExPointFilter::OutputInsideFiltersLabel : Super::GetMainOutputPin();
}

bool FPCGExUberFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	PCGExFactories::GetInputFactories(
		Context, PCGExPicker::SourcePickersLabel, Context->PickerFactories,
		{PCGExFactories::EType::IndexPicker}, false);

	if (Settings->Mode == EPCGExUberFilterMode::Write)
	{
		return Settings->ResultDetails.Validate(Context);
	}

	Context->Inside = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Outside = MakeShared<PCGExData::FPointIOCollection>(Context);

	Context->Inside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;

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

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to filter."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	if (Settings->Mode == EPCGExUberFilterMode::Write)
	{
		Context->MainPoints->StageOutputs();
	}
	else
	{
		Context->Inside->PruneNullEntries(true);
		Context->Outside->PruneNullEntries(true);

		uint64& Mask = Context->OutputData.InactiveOutputPinBitmask;
		if (!Context->Inside->StageOutputs()) { Mask |= 1ULL << 0; }
		if (!Context->Outside->StageOutputs()) { Mask |= 1ULL << 1; }
	}

	return Context->TryComplete();
}

namespace PCGExUberFilter
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExUberFilter::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->Mode == EPCGExUberFilterMode::Write ? PCGExData::EIOInit::Duplicate : PCGExData::EIOInit::NoInit)

		bUsePicks = PCGExPicker::GetPicks(Context->PickerFactories, PointDataFacade, Picks);

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			Results = Settings->ResultDetails;
			Results.Init(PointDataFacade);
		}
		else
		{
			PCGEx::InitArray(PointFilterCache, PointDataFacade->GetNum());
		}

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		if (!Results.bEnabled)
		{
			const int32 MaxRange = PCGExMT::FScope::GetMaxRange(Loops);

			IndicesInside = MakeShared<PCGExMT::TScopedArray<int32>>(Loops);
			IndicesInside->Reserve(MaxRange);

			IndicesOutside = MakeShared<PCGExMT::TScopedArray<int32>>(Loops);
			IndicesOutside->Reserve(MaxRange);
		}
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::UberFilter::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		if (bUsePicks)
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				if (!Picks.Contains(Index)) { PointFilterCache[Index] = Settings->UnpickedFallback == EPCGExFilterFallback::Pass; }
			}
		}

		if (Settings->bSwap)
		{
			PCGEX_SCOPE_LOOP(Index) { PointFilterCache[Index] = !PointFilterCache[Index]; }
		}

		if (!Results.bEnabled)
		{
			TArray<int32>& IndicesInsideRef = IndicesInside->Get_Ref(Scope);
			TArray<int32>& IndicesOutsideRef = IndicesOutside->Get_Ref(Scope);

			PCGEX_SCOPE_LOOP(Index)
			{
				const int8 bPass = PointFilterCache[Index];

				if (bPass) { IndicesInsideRef.Add(Index); }
				else { IndicesOutsideRef.Add(Index); }

				if (bPass) { FPlatformAtomics::InterlockedAdd(&NumInside, 1); }
				else { FPlatformAtomics::InterlockedAdd(&NumOutside, 1); }
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				if (PointFilterCache[Index]) { FPlatformAtomics::InterlockedAdd(&NumInside, 1); }
				else { FPlatformAtomics::InterlockedAdd(&NumOutside, 1); }
			}

			Results.Write(Scope, PointFilterCache);
		}
	}

	TSharedPtr<PCGExData::FPointIO> FProcessor::CreateIO(const TSharedRef<PCGExData::FPointIOCollection>& InCollection, const PCGExData::EIOInit InitMode) const
	{
		TSharedPtr<PCGExData::FPointIO> NewPointIO = PCGExData::NewPointIO(PointDataFacade->Source, InCollection->OutputPin);

		if (!NewPointIO->InitializeOutput(InitMode)) { return nullptr; }

		InCollection->Pairs[BatchIndex] = NewPointIO;
		return NewPointIO;
	}

	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterProcessor::CompleteWork);

		if (Settings->Mode == EPCGExUberFilterMode::Write)
		{
			const bool bHasAnyPass = NumInside != 0;
			const bool bAllPass = NumInside == PointDataFacade->GetNum();
			if (bHasAnyPass && Settings->bTagIfAnyPointPassed) { PointDataFacade->Source->Tags->AddRaw(Settings->HasAnyPointPassedTag); }
			if (bAllPass && Settings->bTagIfAllPointsPassed) { PointDataFacade->Source->Tags->AddRaw(Settings->AllPointsPassedTag); }
			if (!bHasAnyPass && Settings->bTagIfNoPointPassed) { PointDataFacade->Source->Tags->AddRaw(Settings->NoPointPassedTag); }


			PointDataFacade->WriteFastest(AsyncManager);
			return;
		}

		if (NumInside == 0 || NumOutside == 0)
		{
			if (NumInside == 0)
			{
				if (!Settings->bOutputDiscardedElements) { return; }
				Outside = CreateIO(Context->Outside.ToSharedRef(), PCGExData::EIOInit::Forward);
				if (!Outside) { return; }
				if (Settings->bTagIfNoPointPassed) { Outside->Tags->AddRaw(Settings->NoPointPassedTag); }
			}
			else
			{
				Inside = CreateIO(Context->Inside.ToSharedRef(), PCGExData::EIOInit::Forward);
				if (!Inside) { return; }
				if (Settings->bTagIfAnyPointPassed) { Inside->Tags->AddRaw(Settings->HasAnyPointPassedTag); }
				if (Settings->bTagIfAllPointsPassed) { Inside->Tags->AddRaw(Settings->AllPointsPassedTag); }
			}
			return;
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		TArray<int32> Indices;
		PCGEx::InitArray(Indices, NumPoints);

		TArray<int32> ReadIndices;
		IndicesInside->Collapse(ReadIndices);

		Inside = CreateIO(Context->Inside.ToSharedRef(), PCGExData::EIOInit::New);
		if (!Inside) { return; }

		PCGEx::SetNumPointsAllocated(Inside->GetOut(), ReadIndices.Num(), Inside->GetAllocations());
		Inside->InheritProperties(ReadIndices, Inside->GetAllocations());

		if (Settings->bTagIfAnyPointPassed) { Inside->Tags->AddRaw(Settings->HasAnyPointPassedTag); }

		if (!Settings->bOutputDiscardedElements) { return; }

		ReadIndices.Reset();
		IndicesOutside->Collapse(ReadIndices);
		Outside = CreateIO(Context->Outside.ToSharedRef(), PCGExData::EIOInit::New);

		if (!Outside) { return; }

		PCGEx::SetNumPointsAllocated(Outside->GetOut(), ReadIndices.Num(), Outside->GetAllocations());
		Outside->InheritProperties(ReadIndices, Outside->GetAllocations());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

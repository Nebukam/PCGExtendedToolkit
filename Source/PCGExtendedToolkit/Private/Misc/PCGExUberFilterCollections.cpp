﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilterCollections.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"
#include "Misc/Pickers/PCGExPicker.h"
#include "Misc/Pickers/PCGExPickerFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExUberFilterCollections"
#define PCGEX_NAMESPACE UberFilterCollections

#if WITH_EDITOR
bool UPCGExUberFilterCollectionsSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExPicker::SourcePickersLabel) { return InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}
#endif

bool UPCGExUberFilterCollectionsSettings::HasDynamicPins() const { return true; }

TArray<FPCGPinProperties> UPCGExUberFilterCollectionsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExPicker::SourcePickersLabel, "A precise selection of point that will be tested, as opposed to all of them.", Normal, FPCGExDataTypeInfoPicker::AsId())
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUberFilterCollectionsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGExPointFilter::OutputInsideFiltersLabel, "Collections that passed the filters.", Required)
	PCGEX_PIN_ANY(PCGExPointFilter::OutputOutsideFiltersLabel, "Collections that didn't pass the filters.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(UberFilterCollections)
PCGEX_ELEMENT_BATCH_POINT_IMPL(UberFilterCollections)

FName UPCGExUberFilterCollectionsSettings::GetMainOutputPin() const
{
	// Ensure proper forward when node is disabled
	return PCGExPointFilter::OutputInsideFiltersLabel;
}

bool UPCGExUberFilterCollectionsSettings::GetIsMainTransactional() const { return true; }

bool FPCGExUberFilterCollectionsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilterCollections)

	PCGExFactories::GetInputFactories(
		Context, PCGExPicker::SourcePickersLabel, Context->PickerFactories,
		{PCGExFactories::EType::IndexPicker}, false);

	Context->Inside = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Outside = MakeShared<PCGExData::FPointIOCollection>(Context);

	Context->Inside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;

	if (Settings->bSwap)
	{
		Context->Inside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;
		Context->Outside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	}

	Context->bHasOnlyCollectionFilters = true;
	for (const TObjectPtr<const UPCGExPointFilterFactoryData>& FilterFactory : Context->FilterFactories)
	{
		if (!FilterFactory->SupportsCollectionEvaluation())
		{
			Context->bHasOnlyCollectionFilters = false;
			break;
		}
	}

	return true;
}

bool FPCGExUberFilterCollectionsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterCollectionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberFilterCollections)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->bHasOnlyCollectionFilters)
		{
			Context->NumPairs = Context->MainPoints->Pairs.Num();

			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
					NewBatch->bSkipCompletion = Context->bHasOnlyCollectionFilters;
				}))
			{
				return Context->CancelExecution(TEXT("Could not find any points to filter."));
			}
		}
		else
		{
			PCGEX_MAKE_SHARED(DummyFacade, PCGExData::FFacade, Context->MainPoints->Pairs[0].ToSharedRef())
			PCGEX_MAKE_SHARED(PrimaryFilters, PCGExPointFilter::FManager, DummyFacade.ToSharedRef())
			PrimaryFilters->bWillBeUsedWithCollections = true;
			PrimaryFilters->Init(Context, Context->FilterFactories);

			while (Context->AdvancePointsIO())
			{
				if (PrimaryFilters->Test(Context->CurrentIO, Context->MainPoints)) { Context->Inside->Emplace_GetRef(Context->CurrentIO, PCGExData::EIOInit::Forward); }
				else { Context->Outside->Emplace_GetRef(Context->CurrentIO, PCGExData::EIOInit::Forward); }
			}

			Context->Done();
		}
	}

	if (!Context->bHasOnlyCollectionFilters)
	{
		PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)
		Context->MainBatch->Output();
	}

	Context->Inside->StageOutputs();
	Context->Outside->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExUberFilterCollections
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExUberFilterCollections::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PointDataFacade->Source->bAllowEmptyOutput = true;

		bUsePicks = PCGExPicker::GetPicks(Context->PickerFactories, PointDataFacade, Picks);
		NumPoints = bUsePicks ? Picks.Num() : PointDataFacade->GetNum();

		if (Settings->Measure == EPCGExMeanMeasure::Discrete)
		{
			if ((Settings->Comparison == EPCGExComparison::StrictlyGreater ||
					Settings->Comparison == EPCGExComparison::EqualOrGreater) &&
				NumPoints < Settings->IntThreshold)
			{
				// Not enough points to meet requirements.
				Context->Outside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward);
				return true;
			}
		}

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::UberFilterCollections::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		if (bUsePicks)
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				if (!Picks.Contains(Index)) { continue; }
				if (PointFilterCache[Index]) { FPlatformAtomics::InterlockedAdd(&NumInside, 1); }
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
		}
	}

	void FProcessor::Output()
	{
		IProcessor::Output();

		switch (Settings->Mode)
		{
		default:
		case EPCGExUberFilterCollectionsMode::All:
			if (NumInside == NumPoints) { Context->Inside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			else { Context->Outside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			break;
		case EPCGExUberFilterCollectionsMode::Any:
			if (NumInside != 0) { Context->Inside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			else { Context->Outside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			break;
		case EPCGExUberFilterCollectionsMode::Partial:
			if (Settings->Measure == EPCGExMeanMeasure::Discrete)
			{
				if (PCGExCompare::Compare(Settings->Comparison, NumInside, Settings->IntThreshold, 0)) { Context->Inside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
				else { Context->Outside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			}
			else
			{
				const double Ratio = static_cast<double>(NumInside) / static_cast<double>(NumPoints);
				if (PCGExCompare::Compare(Settings->Comparison, Ratio, Settings->DblThreshold, Settings->Tolerance)) { Context->Inside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
				else { Context->Outside->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			}
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

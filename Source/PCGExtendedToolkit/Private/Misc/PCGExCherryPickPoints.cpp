// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCherryPickPoints.h"


#include "Misc/PCGExDiscardByPointCount.h"
#include "Misc/Pickers/PCGExPicker.h"

#define LOCTEXT_NAMESPACE "PCGExCherryPickPointsElement"
#define PCGEX_NAMESPACE CherryPickPoints

PCGEX_INITIALIZE_ELEMENT(CherryPickPoints)

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPicker::SourcePickersLabel, "Pickers config", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (bOutputDiscardedPoints) { PCGEX_PIN_POINTS(PCGExDiscardByPointCount::OutputDiscardedLabel, "Discarded points", Normal, {}) }
	return PinProperties;
}

bool FPCGExCherryPickPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	PCGExFactories::GetInputFactories(Context, PCGExPicker::SourcePickersLabel, Context->PickerFactories, {PCGExFactories::EType::IndexPicker}, false);
	if (Context->PickerFactories.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing pickers."));
		return false;
	}

	return true;
}

bool FPCGExCherryPickPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCherryPickPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExCherryPickPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExCherryPickPoints::FProcessor>>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any data to cherry pick."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExCherryPickPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCherryPickPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		TSet<int32> UniqueIndices;

		for (const TObjectPtr<const UPCGExPickerFactoryData>& Op : Context->PickerFactories)
		{
			Op->AddPicks(PointDataFacade->GetNum(), UniqueIndices);
		}

		if (UniqueIndices.IsEmpty())
		{
			PointDataFacade->Source->Disable();
			return true;
		}

		if (!PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::New)) { return false; }

		PointDataFacade->Source->bAllowEmptyOutput = Settings->bAllowEmptyOutputs;

		const TArray<FPCGPoint>& SourcePoints = PointDataFacade->GetIn()->GetPoints();

		const int32 NumPoints = SourcePoints.Num();
		const int32 NumPicks = FMath::Min(UniqueIndices.Num(), NumPoints);

		if (Settings->bInvert || Settings->bOutputDiscardedPoints)
		{
			// Work from set

			if (Settings->bOutputDiscardedPoints)
			{
				const TSharedPtr<PCGExData::FPointIO> Discarded = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);

				if (!Discarded) { return false; }

				Discarded->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
				Discarded->IOIndex = PointDataFacade->Source->IOIndex;

				Discarded->bAllowEmptyOutput = Settings->bAllowEmptyOutputs;

				TArray<FPCGPoint>& PickedPoints = Settings->bInvert ? Discarded->GetMutablePoints() : PointDataFacade->GetMutablePoints();
				TArray<FPCGPoint>& DiscardedPoints = Settings->bInvert ? PointDataFacade->GetMutablePoints() : Discarded->GetMutablePoints();

				PickedPoints.Reserve(NumPicks);
				DiscardedPoints.Reserve(NumPoints - NumPicks);

				for (int32 i = 0; i < NumPoints; i++)
				{
					if (UniqueIndices.Contains(i)) { PickedPoints.Add(SourcePoints[i]); }
					else { DiscardedPoints.Add(SourcePoints[i]); }
				}
			}
			else
			{
				TArray<FPCGPoint>& PickedPoints = PointDataFacade->GetMutablePoints();
				PickedPoints.Reserve(NumPoints - NumPicks);
				for (int32 i = 0; i < NumPoints; i++) { if (!UniqueIndices.Contains(i)) { PickedPoints.Add(SourcePoints[i]); } }
			}
		}
		else
		{
			// Work from indices

			TArray<FPCGPoint>& PickedPoints = PointDataFacade->GetMutablePoints();

			TArray<int32> PickedIndices = UniqueIndices.Array();
			PickedIndices.Sort();


			PickedPoints.Reserve(NumPicks);

			for (int32 i = 0; i < NumPicks; i++) { PickedPoints.Add(SourcePoints[PickedIndices[i]]); }
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

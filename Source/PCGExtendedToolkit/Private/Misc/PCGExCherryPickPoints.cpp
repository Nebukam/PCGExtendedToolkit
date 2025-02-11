// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCherryPickPoints.h"

#include "Misc/Pickers/PCGExPicker.h"
#include "Misc/Pickers/PCGExPickerOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCherryPickPointsElement"
#define PCGEX_NAMESPACE CherryPickPoints

PCGExData::EIOInit UPCGExCherryPickPointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(CherryPickPoints)

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPicker::SourcePickersLabel, "Pickers config", Required, {})
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

	Context->PickerOperations.Reserve(Context->PickerFactories.Num());

	for (const TObjectPtr<const UPCGExPickerFactoryData>& PickerFactory : Context->PickerFactories)
	{
		UPCGExPickerOperation* Op = PickerFactory->CreateOperation(Context);
		Op->BindContext(Context);
		Context->PickerOperations.Add(Op);
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
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCherryPickPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		TSet<int32> UniqueIndices;

		for (const UPCGExPickerOperation* Op : Context->PickerOperations)
		{
			Op->AddPicks(PointDataFacade, UniqueIndices);
		}

		if (UniqueIndices.IsEmpty())
		{
			PointDataFacade->Source->Disable();
			return true;
		}

		if (!PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::New)) { return false; }

		PickedIndices = UniqueIndices.Array();
		PickedIndices.Sort();

		const TArray<FPCGPoint>& SourcePoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetMutablePoints();

		const int32 NumPicks = PickedIndices.Num();
		MutablePoints.Reserve(NumPicks);

		for (int32 i = 0; i < NumPicks; i++) { MutablePoints.Add(SourcePoints[PickedIndices[i]]); }

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

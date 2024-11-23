// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCherryPickPoints.h"


#define LOCTEXT_NAMESPACE "PCGExCherryPickPointsElement"
#define PCGEX_NAMESPACE CherryPickPoints

PCGExData::EIOInit UPCGExCherryPickPointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(CherryPickPoints)

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (IndicesSource == EPCGExCherryPickSource::Target) { PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "TBD", Required, {}) }
	return PinProperties;
}

bool FPCGExCherryPickPointsContext::TryGetUniqueIndices(const TSharedRef<PCGExData::FPointIO>& InSource, TArray<int32>& OutUniqueIndices, const int32 MaxIndex) const
{
	PCGEX_SETTINGS_LOCAL(CherryPickPoints)

	TArray<int32> SourceIndices;
	TSet<int32> UniqueIndices;
	const TUniquePtr<PCGEx::TAttributeBroadcaster<int32>> Getter = MakeUnique<PCGEx::TAttributeBroadcaster<int32>>();
	if (!Getter->Prepare(Settings->ReadIndexFromAttribute, InSource))
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Index attribute is invalid."));
		return false;
	}

	int32 Min = 0;
	int32 Max = 0;

	Getter->GrabAndDump(SourceIndices, false, Min, Max);

	if (MaxIndex == -1)
	{
		for (const int32 Value : Getter->Values)
		{
			if (Value < 0) { continue; }
			UniqueIndices.Add(Value);
		}
	}
	else
	{
		for (int32& Value : Getter->Values)
		{
			Value = PCGExMath::SanitizeIndex(Value, MaxIndex, Settings->Safety);
			if (Value < 0) { continue; }
			UniqueIndices.Add(Value);
		}
	}

	OutUniqueIndices.Reserve(UniqueIndices.Num());
	OutUniqueIndices.Append(UniqueIndices.Array());
	OutUniqueIndices.Sort();

	return true;
}

bool FPCGExCherryPickPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	if (Settings->IndicesSource == EPCGExCherryPickSource::Target)
	{
		const TSharedPtr<PCGExData::FPointIO> Targets = PCGExData::TryGetSingleInput(Context, PCGEx::SourceTargetsLabel, true);
		if (!Targets) { return false; }
		if (!Context->TryGetUniqueIndices(Targets.ToSharedRef(), Context->SharedTargetIndices)) { return false; }
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

		const int32 MaxIndex = PointDataFacade->GetNum() - 1;
		if (Settings->IndicesSource == EPCGExCherryPickSource::Self)
		{
			if (!Context->TryGetUniqueIndices(PointDataFacade->Source, PickedIndices, MaxIndex)) { return false; }
		}
		else
		{
			for (const int32 Value : Context->SharedTargetIndices)
			{
				const int32 SanitizedIndex = PCGExMath::SanitizeIndex(Value, MaxIndex, Settings->Safety);
				if (SanitizedIndex < 0) { continue; }
				PickedIndices.Add(SanitizedIndex);
			}
		}

		if (PickedIndices.IsEmpty()) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::New);
		const TArray<FPCGPoint>& PickablePoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		const int32 NumPicked = PickedIndices.Num();

		MutablePoints.SetNumUninitialized(NumPicked);
		for (int i = 0; i < NumPicked; i++) { MutablePoints[i] = PickablePoints[PickedIndices[i]]; }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

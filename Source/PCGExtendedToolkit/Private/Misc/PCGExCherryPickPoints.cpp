// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCherryPickPoints.h"

#define LOCTEXT_NAMESPACE "PCGExCherryPickPointsElement"
#define PCGEX_NAMESPACE CherryPickPoints

PCGExData::EInit UPCGExCherryPickPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(CherryPickPoints)

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (IndicesSource == EPCGExCherryPickSource::Target) { PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "TBD", Required, {}) }
	return PinProperties;
}

FPCGExCherryPickPointsContext::~FPCGExCherryPickPointsContext()
{
}

bool FPCGExCherryPickPointsContext::TryGetUniqueIndices(const PCGExData::FPointIO* InSource, TArray<int32>& OutUniqueIndices, int32 MaxIndex) const
{
	PCGEX_SETTINGS_LOCAL(CherryPickPoints)

	TArray<int32> SourceIndices;
	TSet<int32> UniqueIndices;
	PCGEx::FLocalIntegerGetter* Getter = new PCGEx::FLocalIntegerGetter();
	Getter->Capture(Settings->ReadIndexFromAttribute);

	int32 Min = 0;
	int32 Max = 0;

	if (!Getter->GrabAndDump(InSource, SourceIndices, false, Min, Max))
	{
		PCGEX_DELETE(Getter)
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Index attribute is invalid."));
		return false;
	}

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

	PCGEX_DELETE(Getter)

	OutUniqueIndices.Reserve(UniqueIndices.Num());
	OutUniqueIndices.Append(UniqueIndices.Array());
	OutUniqueIndices.Sort();

	return true;
}

bool FPCGExCherryPickPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	if (Settings->IndicesSource == EPCGExCherryPickSource::Target)
	{
		const PCGExData::FPointIO* Targets = PCGExData::TryGetSingleInput(Context, PCGEx::SourceTargetsLabel, true);
		if (!Targets) { return false; }

		if (!Context->TryGetUniqueIndices(Targets, Context->SharedTargetIndices))
		{
			PCGEX_DELETE(Targets)
			return false;
		}

		PCGEX_DELETE(Targets)
	}


	return true;
}

bool FPCGExCherryPickPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCherryPickPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExCherryPickPoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExCherryPickPoints::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any data to cherry pick."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExCherryPickPoints
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCherryPickPoints::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CherryPickPoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		const int32 MaxIndex = PointIO->GetNum() - 1;
		if (Settings->IndicesSource == EPCGExCherryPickSource::Self)
		{
			if (!TypedContext->TryGetUniqueIndices(PointIO, PickedIndices, MaxIndex)) { return false; }
		}
		else
		{
			for (const int32 Value : TypedContext->SharedTargetIndices)
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
		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);
		const TArray<FPCGPoint>& PickablePoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		const int32 NumPicked = PickedIndices.Num();

		MutablePoints.SetNumUninitialized(NumPicked);
		for (int i = 0; i < NumPicked; i++) { MutablePoints[i] = PickablePoints[PickedIndices[i]]; }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

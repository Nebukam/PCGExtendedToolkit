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

bool FPCGExCherryPickPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	if (Settings->IndicesSource == EPCGExCherryPickSource::Target)
	{
		PCGExData::FPointIO* Targets = PCGExData::TryGetSingleInput(Context, PCGEx::SourceTargetsLabel, true);
		if (!Targets) { return false; }

		TSet<int32> UniqueIndices;
		PCGEx::FLocalSingleFieldGetter* Getter = new PCGEx::FLocalSingleFieldGetter();
		Getter->Capture(Settings->ReadIndexFromAttribute);

		if (!Getter->Grab(Targets))
		{
			PCGEX_DELETE(Targets)
			PCGEX_DELETE(Getter)
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Could not find any paths to fuse."));
			return false;
		}

		for (const double Value : Getter->Values)
		{
			if (Value < 0) { continue; }
			UniqueIndices.Add(Value);
		}

		PCGEX_DELETE(Targets)
		PCGEX_DELETE(Getter)

		Context->SharedTargetIndices.Append(UniqueIndices.Array());
		Context->SharedTargetIndices.Sort();
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

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExCherryPickPoints
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CherryPickPoints)
		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		if (Settings->IndicesSource == EPCGExCherryPickSource::Self)
		{
			TSet<int32> UniqueIndices;
			PCGEx::FLocalSingleFieldGetter* Getter = new PCGEx::FLocalSingleFieldGetter();
			Getter->Capture(Settings->ReadIndexFromAttribute);
			if (!Getter->Grab(PointIO))
			{
				PCGEX_DELETE(Getter)
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Could not find any paths to fuse."));
				return false;
			}

			const int32 MaxIndex = PointIO->GetNum();
			for (const double Value : Getter->Values)
			{
				if (!FMath::IsWithin(Value, 0, MaxIndex)) { continue; }
				UniqueIndices.Add(Value);
			}

			PCGEX_DELETE(Getter)

			PickedIndices.Reserve(UniqueIndices.Num());
			PickedIndices.Append(UniqueIndices.Array());
			PickedIndices.Sort();
		}
		else
		{
			const int32 MaxIndex = PointIO->GetNum();
			for (const double Value : TypedContext->SharedTargetIndices)
			{
				if (Value >= MaxIndex) { continue; }
				PickedIndices.Add(Value);
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

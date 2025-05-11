// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCopyToPoints.h"

#define LOCTEXT_NAMESPACE "PCGExCopyToPointsElement"
#define PCGEX_NAMESPACE CopyToPoints

TArray<FPCGPinProperties> UPCGExCopyToPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Target points to copy inputs to.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(CopyToPoints)

bool FPCGExCopyToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyToPoints)

	Context->TargetsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, false, true);
	if (!Context->TargetsDataFacade) { return false; }

	PCGEX_FWD(TransformDetails)
	if (!Context->TransformDetails.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }

	PCGEX_FWD(TargetsAttributesToClusterTags)
	if (!Context->TargetsAttributesToClusterTags.Init(Context, Context->TargetsDataFacade)) { return false; }

	if (Settings->bDoMatchByTags)
	{
		PCGEX_FWD(MatchByTagValue)
		if (!Context->MatchByTagValue.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }
	}

	Context->TargetsForwardHandler = Settings->TargetsForwarding.GetHandler(Context->TargetsDataFacade);

	return true;
}

bool FPCGExCopyToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyToPoints)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExCopyToPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExCopyToPoints::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExCopyToPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCopyToPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TArray<FPCGPoint>& Targets = Context->TargetsDataFacade->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		PCGEx::InitArray(Dupes, NumTargets);

		for (int i = 0; i < NumTargets; i++)
		{
			Dupes[i] = nullptr;

			if (Settings->bDoMatchByTags &&
				!Context->MatchByTagValue.Matches(PointDataFacade->Source->Tags, Context->TargetsDataFacade->Source->GetInPointRef(i)))
			{
				continue;
			}

			NumCopies++;

			TSharedPtr<PCGExData::FPointIO> Dupe = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Duplicate);

			Dupes[i] = Dupe;
			PCGEX_LAUNCH(PCGExGeoTasks::FTransformPointIO, i, Context->TargetsDataFacade->Source, Dupe, &Context->TransformDetails)
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (NumCopies == 0) { return; }

		const TArray<FPCGPoint>& Targets = Context->TargetsDataFacade->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		// Once work is complete, check if there are cached clusters we can forward
		for (int i = 0; i < NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = Dupes[i];

			if (!EdgeDupe) { continue; }

			Context->TargetsAttributesToClusterTags.Tag(i, EdgeDupe);
			Context->TargetsForwardHandler->Forward(i, EdgeDupe->GetOut()->Metadata);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

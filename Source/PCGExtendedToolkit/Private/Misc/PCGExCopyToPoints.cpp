// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCopyToPoints.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoTasks.h"

#define LOCTEXT_NAMESPACE "PCGExCopyToPointsElement"
#define PCGEX_NAMESPACE CopyToPoints

TArray<FPCGPinProperties> UPCGExCopyToPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Target points to copy inputs to.", Required)
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(DataMatching, PinProperties);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCopyToPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExMatching::Helpers::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(CopyToPoints)
PCGEX_ELEMENT_BATCH_POINT_IMPL(CopyToPoints)

bool FPCGExCopyToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyToPoints)

	Context->TargetsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, false, true);
	if (!Context->TargetsDataFacade) { return false; }

	PCGEX_FWD(TransformDetails)
	if (!Context->TransformDetails.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }

	PCGEX_FWD(TargetsAttributesToCopyTags)
	if (!Context->TargetsAttributesToCopyTags.Init(Context, Context->TargetsDataFacade)) { return false; }

	Context->DataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
	Context->DataMatcher->SetDetails(&Settings->DataMatching);
	if (!Context->DataMatcher->Init(Context, {Context->TargetsDataFacade}, true)) { return false; }


	Context->TargetsForwardHandler = Settings->TargetsForwarding.GetHandler(Context->TargetsDataFacade);

	return true;
}

bool FPCGExCopyToPointsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyToPoints)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExCopyToPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCopyToPoints::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		MatchScope = PCGExMatching::FMatchingScope(Context->InitialMainPointsNum);

		const UPCGBasePointData* Targets = Context->TargetsDataFacade->GetIn();
		const int32 NumTargets = Targets->GetNumPoints();

		PCGExArrayHelpers::InitArray(Dupes, NumTargets);

		StartParallelLoopForRange(NumTargets, 32);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		int32 Copies = 0;
		PCGEX_SCOPE_LOOP(i)
		{
			Dupes[i] = nullptr;

			if (!Context->DataMatcher->Test(Context->TargetsDataFacade->GetInPoint(i), PointDataFacade->Source, MatchScope)) { continue; }

			Copies++;

			TSharedPtr<PCGExData::FPointIO> Dupe = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Duplicate);
			Context->TargetsForwardHandler->Forward(i, Dupe->GetOut()->Metadata);

			Dupes[i] = Dupe;

			PCGEX_LAUNCH(PCGExGeoTasks::FTransformPointIO, i, Context->TargetsDataFacade->Source, Dupe, &Context->TransformDetails)
		}

		if (Copies > 0) { FPlatformAtomics::InterlockedAdd(&NumCopies, Copies); }
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->DataMatching.bSplitUnmatched && NumCopies == 0)
		{
			(void)Context->DataMatcher->HandleUnmatchedOutput(PointDataFacade, true);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

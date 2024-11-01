// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

PCGExData::EInit UPCGExFuseCollinearSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)

bool FPCGExFuseCollinearElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	Context->DotThreshold = Settings->bInvertThreshold ? PCGExMath::DegreesToDot(180 - Settings->Threshold) : PCGExMath::DegreesToDot(Settings->Threshold);
	Context->FuseDistSquared = Settings->FuseDistance * Settings->FuseDistance;

	return true;
}


bool FPCGExFuseCollinearElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseCollinearElement::Execute);

	PCGEX_CONTEXT(FuseCollinear)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFuseCollinear
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseCollinear::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		MaxIndex = PointDataFacade->GetNum() - 1;
		PointDataFacade->Source->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();
		OutPoints->Reserve(MaxIndex + 1);
		OutPoints->Add(InPoints[0]);

		LastPosition = InPoints[0].Transform.GetLocation();
		CurrentDirection = (InPoints[1].Transform.GetLocation() - LastPosition).GetSafeNormal();

		bInlineProcessPoints = true;
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
		if (Index == 0 || Index == MaxIndex) { return; }

		const FVector CurrentPosition = Point.Transform.GetLocation();
		const FVector NextPosition = PointDataFacade->Source->GetInPoint(Index + 1).Transform.GetLocation();
		const FVector DirToNext = (NextPosition - CurrentPosition).GetSafeNormal();

		const double Dot = FVector::DotProduct(CurrentDirection, DirToNext);
		const bool bWithinThreshold = Dot > Context->DotThreshold;
		if (FVector::DistSquared(CurrentPosition, LastPosition) <= Context->FuseDistSquared || bWithinThreshold)
		{
			// Collinear with previous, keep moving
			return;
		}

		OutPoints->Add_GetRef(Point);
		CurrentDirection = DirToNext;
		LastPosition = CurrentPosition;
	}

	void FProcessor::CompleteWork()
	{
		OutPoints->Add(PointDataFacade->Source->GetInPoint(MaxIndex));
		OutPoints->Shrink();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

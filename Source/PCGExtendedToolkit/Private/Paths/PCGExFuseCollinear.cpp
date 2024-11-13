// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

PCGExData::EIOInit UPCGExFuseCollinearSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)

bool FPCGExFuseCollinearElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	Context->DotThreshold = PCGExMath::DegreesToDot(Settings->Threshold);
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
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
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

		Path = PCGExPaths::MakePath(
			PointDataFacade->Source->GetIn()->GetPoints(), 0,
			Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source));

		PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();
		OutPoints->Reserve(Path->NumPoints);
		OutPoints->Add(InPoints[0]);

		LastPosition = Path->GetPos(0);

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
#define PCGEX_INSERT_CURRENT_POINT\
		OutPoints->Add_GetRef(Point);\
		LastPosition = Path->GetPos(Index);

		if (PointFilterCache[Index])
		{
			// Kept point, as per filters
			PCGEX_INSERT_CURRENT_POINT
			return;
		}

		if (Index == 0) { return; }

		if (Settings->bFuseCollocated && FVector::DistSquared(LastPosition, Path->GetPos(Index)) <= Context->FuseDistSquared)
		{
			// Collocated points
			return;
		}

		const double Dot = FVector::DotProduct(Path->DirToPrevPoint(Index) * -1, Path->DirToNextPoint(Index));
		if ((!Settings->bInvertThreshold && Dot > Context->DotThreshold) ||
			(Settings->bInvertThreshold && Dot < Context->DotThreshold))
		{
			// Collinear with previous, keep moving
			return;
		}

		PCGEX_INSERT_CURRENT_POINT

#undef PCGEX_INSERT_CURRENT_POINT
	}

	void FProcessor::CompleteWork()
	{
		if (Path->IsClosedLoop())
		{

				const double Dot = FVector::DotProduct(Path->DirToPrevPoint(0) * -1, Path->DirToNextPoint(0));
				if ((!Settings->bInvertThreshold && Dot > Context->DotThreshold) ||
					(Settings->bInvertThreshold && Dot < Context->DotThreshold))
				{
					OutPoints->RemoveAt(0);
				}
		}
		
		OutPoints->Shrink();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#include "PCGExMath.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)
PCGEX_ELEMENT_BATCH_POINT_IMPL(FuseCollinear)

bool FPCGExFuseCollinearElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	Context->DotThreshold = PCGExMath::DegreesToDot(Settings->Threshold);
	Context->FuseDistSquared = FMath::Square(Settings->FuseDistance);

	return true;
}


bool FPCGExFuseCollinearElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseCollinearElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					if (!Settings->bOmitInvalidPathsFromOutput) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExFuseCollinear
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseCollinear::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);

		ReadIndices.Reserve(Path->NumPoints);
		LastPosition = Path->GetPos(0);

		bDaisyChainProcessPoints = true;
		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::FuseCollinear::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		// Preserve start & end
		PointFilterCache[0] = true;
		if (!Path->IsClosedLoop()) { PointFilterCache[Path->LastIndex] = true; } // Don't force-preserve last point if closed loop

		PCGEX_SCOPE_LOOP(Index)
		{
			if (PointFilterCache[Index])
			{
				// Kept point, as per filters
				ReadIndices.Add(Index);
				LastPosition = Path->GetPos(Index);
				continue;
			}

			const FVector CurrentPos = Path->GetPos(Index);
			if (Settings->bFuseCollocated && FVector::DistSquared(LastPosition, CurrentPos) <= Context->FuseDistSquared)
			{
				// Collocated points
				continue;
			}

			// Use last position to avoid removing smooth arcs
			const double Dot = FVector::DotProduct((CurrentPos - LastPosition).GetSafeNormal(), Path->DirToNextPoint(Index));
			if ((!Settings->bInvertThreshold && Dot > Context->DotThreshold) ||
				(Settings->bInvertThreshold && Dot < Context->DotThreshold))
			{
				// Collinear with previous, keep moving
				continue;
			}

			ReadIndices.Add(Index);
			LastPosition = Path->GetPos(Index);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (ReadIndices.Num() < 2) { return; }

		PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New)

		PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), ReadIndices.Num(), PointDataFacade->GetAllocations());
		PointDataFacade->Source->InheritPoints(ReadIndices, 0);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

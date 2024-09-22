// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

PCGExData::EInit UPCGExFuseCollinearSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)

FPCGExFuseCollinearContext::~FPCGExFuseCollinearContext()
{
	PCGEX_TERMINATE_ASYNC
}


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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExFuseCollinear::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExFuseCollinear
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseCollinear::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseCollinear)

		PointDataFacade->bSupportsScopedGet = TypedContext->bScopedAttributeGet;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;

		MaxIndex = PointIO->GetNum() - 1;
		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		OutPoints = &PointIO->GetOut()->GetMutablePoints();
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
		const FVector NextPosition = PointIO->GetInPoint(Index + 1).Transform.GetLocation();
		const FVector DirToNext = (NextPosition - CurrentPosition).GetSafeNormal();

		const double Dot = FVector::DotProduct(CurrentDirection, DirToNext);
		const bool bWithinThreshold = Dot > LocalTypedContext->DotThreshold;
		if (FVector::DistSquared(CurrentPosition, LastPosition) <= LocalTypedContext->FuseDistSquared || bWithinThreshold)
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
		OutPoints->Add(PointIO->GetInPoint(MaxIndex));
		OutPoints->Shrink();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

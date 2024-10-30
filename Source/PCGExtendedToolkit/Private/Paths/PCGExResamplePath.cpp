// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExResamplePath.h"

#include "PCGExDataMath.h"

#define LOCTEXT_NAMESPACE "PCGExResamplePathElement"
#define PCGEX_NAMESPACE ResamplePath

PCGExData::EInit UPCGExResamplePathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(ResamplePath)

bool FPCGExResamplePathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ResamplePath)

	return true;
}

bool FPCGExResamplePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExResamplePathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ResamplePath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExResamplePath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExResamplePath::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExResamplePath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExResamplePath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = false; //Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();

		Path = PCGExPaths::MakePath(InPoints, 0, Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source));
		Path->IOIndex = PointDataFacade->Source->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>(true); // Force compute length


		if (Settings->Mode == EPCGExResampleMode::Sweep)
		{
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NewOutput);
			if (Settings->ResolutionMode == EPCGExResolutionMode::Fixed)
			{
				NumSamples = Settings->Resolution;
			}
			else
			{
				NumSamples = PCGEx::TruncateDbl(PathLength->TotalLength / Settings->Resolution, Settings->Truncate);
			}

			TArray<FPCGPoint>& OutPoints = PointDataFacade->GetMutablePoints();
			OutPoints.SetNum(NumSamples);
			OutPoints[0] = PointDataFacade->Source->GetIn()->GetPoints()[0]; // Copy first point
		}
		else
		{
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::DuplicateInput);
			NumSamples = PointDataFacade->GetNum();
		}

		if (Path->IsClosedLoop()) { SampleLength = PathLength->TotalLength / static_cast<double>(NumSamples); }
		else { SampleLength = PathLength->TotalLength / static_cast<double>(NumSamples - 1); }


		Samples.SetNumUninitialized(NumSamples);
		bInlineProcessPoints = true;

		int32 StartIndex = 0;
		int32 EndIndex = 1;
		FVector PrevPosition = InPoints[0].Transform.GetLocation();

		FPointSample& FirstSample = Samples[0];
		FirstSample.Start = StartIndex;
		FirstSample.End = EndIndex;
		FirstSample.Location = PrevPosition;

		for (int i = 1; i < NumSamples; i++)
		{
			FPointSample& Sample = Samples[i];
			Sample.Start = StartIndex;

			FVector NextPosition = InPoints[EndIndex].Transform.GetLocation();
			double DistToNext = FVector::Dist(PrevPosition, NextPosition);
			double Remainder = SampleLength - DistToNext;

			if (Remainder <= 0)
			{
				// Overshooting
				PrevPosition = PrevPosition + (Path->DirToNextPoint(StartIndex) * SampleLength);
			}
			else
			{
				PrevPosition = NextPosition;

				while (Remainder > 0)
				{
					StartIndex = EndIndex++;

					if (EndIndex >= InPoints.Num()) { break; }
					
					NextPosition = InPoints[EndIndex].Transform.GetLocation();
					DistToNext = FVector::Dist(PrevPosition, NextPosition);

					if (Remainder <= DistToNext) { PrevPosition = PrevPosition + (Path->DirToPrevPoint(EndIndex) * -Remainder); }
					else { PrevPosition = NextPosition; }

					Remainder -= DistToNext;
				}
			}

			Sample.End = EndIndex;
			Sample.Location = PrevPosition;
		}

		if (Settings->bPreserveLastPoint)
		{
			FPointSample& LastSample = Samples.Last();
			LastSample.Start = InPoints.Num() - 2;
			LastSample.End = InPoints.Num() - 1;
			LastSample.Location = InPoints.Last().Transform.GetLocation();
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const FPointSample& Sample = Samples[Index];
		Point.Transform.SetLocation(Sample.Location);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

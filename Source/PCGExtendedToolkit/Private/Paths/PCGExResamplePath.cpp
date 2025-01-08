// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExResamplePath.h"

#include "PCGExDataMath.h"

#define LOCTEXT_NAMESPACE "PCGExResamplePathElement"
#define PCGEX_NAMESPACE ResamplePath

PCGExData::EIOInit UPCGExResamplePathSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

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
			if (Settings->ResolutionMode == EPCGExResolutionMode::Fixed)
			{
				NumSamples = Settings->Resolution;
			}
			else
			{
				NumSamples = PCGEx::TruncateDbl(PathLength->TotalLength / Settings->Resolution, Settings->Truncate);
			}

			if (Path->IsClosedLoop()) { NumSamples++; }

			if (NumSamples < 2) { return false; }

			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

			TArray<FPCGPoint>& OutPoints = PointDataFacade->GetMutablePoints();
			OutPoints.SetNum(NumSamples);
			OutPoints[0] = PointDataFacade->Source->GetIn()->GetPoints()[0]; // Copy first point
		}
		else
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
			NumSamples = PointDataFacade->GetNum();
		}

		SampleLength = PathLength->TotalLength / static_cast<double>(NumSamples - 1);

		Samples.SetNumUninitialized(NumSamples);
		bInlineProcessPoints = true;

		int32 StartIndex = 0;
		int32 EndIndex = 1;
		FVector PrevPosition = InPoints[0].Transform.GetLocation();
		double TraversedDistance = 0;

		FPointSample& FirstSample = Samples[0];
		FirstSample.Start = StartIndex;
		FirstSample.End = EndIndex;
		FirstSample.Location = PrevPosition;
		FirstSample.Distance = TraversedDistance;

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
				TraversedDistance += SampleLength;
			}
			else
			{
				PrevPosition = NextPosition;

				while (Remainder > 0)
				{
					StartIndex = EndIndex++;

					if (EndIndex >= InPoints.Num())
					{
						if (!Path->IsClosedLoop())
						{
							EndIndex = InPoints.Num() - 1;
							break;
						}

						EndIndex = 0;
					}

					NextPosition = InPoints[EndIndex].Transform.GetLocation();
					DistToNext = FVector::Dist(PrevPosition, NextPosition);

					if (Remainder <= DistToNext) { PrevPosition = PrevPosition + (Path->DirToPrevPoint(EndIndex) * -Remainder); }
					else { PrevPosition = NextPosition; }
					Remainder -= DistToNext;
				}
			}

			Sample.End = EndIndex;
			Sample.Location = PrevPosition;
			Sample.Distance = TraversedDistance;
		}

		if (Settings->bPreserveLastPoint && !Path->IsClosedLoop())
		{
			FPointSample& LastSample = Samples.Last();
			LastSample.Start = InPoints.Num() - 2;
			LastSample.End = InPoints.Num() - 1;
			LastSample.Location = InPoints.Last().Transform.GetLocation();
			LastSample.Distance = TraversedDistance;
		}

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const FPointSample& Sample = Samples[Index];
		Point.Transform.SetLocation(Sample.Location);

		//if (SourcesRange == 1)
		//{
		const double Weight = FVector::DistSquared(Path->GetPos(Sample.Start), Sample.Location) / FVector::DistSquared(Path->GetPos(Sample.Start), Path->GetPos(Sample.End));
		MetadataBlender->PrepareForBlending(Index);
		MetadataBlender->Blend(Index, Sample.Start, Index, Weight);
		MetadataBlender->Blend(Index, Sample.End, Index, 1 - Weight);
		MetadataBlender->CompleteBlending(Index, 2, 1);
		//}

		/*
		// TODO : Complex blending
		const double MinLength = Sample.Start == 0 ? 0 : PathLength->CumulativeLength[Sample.Start - 1];
		const double MaxLength = PathLength->CumulativeLength[Path->IsValidEdgeIndex(Sample.End) ? PathLength->TotalLength : Sample.End - 1];
		const double Range = MaxLength - MinLength;
		
		for (int i = 0; i < SourcesRange; i++)
		{
			const int 
			PCGExPaths::FPathEdge& Edge = Path->Edges[Sample.Start + i - 1];
			const double Weight =  
		}

		if (Path->IsValidEdgeIndex(Sample.End))
		{
			// Blend with end
		}
		*/
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

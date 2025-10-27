﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExResamplePath.h"

#include "Data/PCGExPointIO.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExResamplePathElement"
#define PCGEX_NAMESPACE ResamplePath

PCGEX_INITIALIZE_ELEMENT(ResamplePath)

PCGExData::EIOInit UPCGExResamplePathSettings::GetMainDataInitializationPolicy() const
{
	if (Mode == EPCGExResampleMode::Sweep) { return PCGExData::EIOInit::New; }
	else { return PCGExData::EIOInit::Duplicate; }
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(ResamplePath)

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
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExResamplePath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExResamplePath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = false; //Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const UPCGBasePointData* InPoints = PointDataFacade->GetIn();

		Path = MakeShared<PCGExPaths::FPath>(InPoints, 0);
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
				NumSamples = PCGExMath::TruncateDbl(PathLength->TotalLength / Settings->Resolution, Settings->Truncate);
			}

			if (Path->IsClosedLoop()) { NumSamples++; }

			if (NumSamples < 2) { return false; }

			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)
			PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), NumSamples, PointDataFacade->GetAllocations());
		}
		else
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
			PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);
			NumSamples = PointDataFacade->GetNum();
		}

		SampleLength = PathLength->TotalLength / static_cast<double>(NumSamples - 1);

		Samples.SetNumUninitialized(NumSamples);
		bForceSingleThreadedProcessPoints = true;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		int32 StartIndex = 0;
		int32 EndIndex = 1;
		FVector PrevPosition = InTransforms[0].GetLocation();
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

			FVector NextPosition = InTransforms[EndIndex].GetLocation();
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

					if (EndIndex >= InTransforms.Num())
					{
						if (!Path->IsClosedLoop())
						{
							EndIndex = InTransforms.Num() - 1;
							break;
						}

						EndIndex = 0;
					}

					NextPosition = InTransforms[EndIndex].GetLocation();
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
			LastSample.Start = InTransforms.Num() - 2;
			LastSample.End = InTransforms.Num() - 1;
			LastSample.Location = InTransforms[LastSample.End].GetLocation();
			LastSample.Distance = TraversedDistance;
		}

		if (Settings->Mode == EPCGExResampleMode::Sweep)
		{
			// Blender will take care of setting all the properties and stuff			
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
			MetadataBlender->SetSourceData(PointDataFacade);
			MetadataBlender->SetTargetData(PointDataFacade);
			if (!MetadataBlender->Init(Context, Settings->BlendingSettings, nullptr, false, PCGExData::EIOSide::In))
			{
				return false;
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ResamplePath::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		if (Settings->Mode == EPCGExResampleMode::Redistribute)
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				const FPointSample& Sample = Samples[Index];
				OutTransforms[Index].SetLocation(Sample.Location);
			}
		}
		else
		{
			TArray<PCGEx::FOpStats> Trackers;
			MetadataBlender->InitTrackers(Trackers);

			PCGEX_SCOPE_LOOP(Index)
			{
				const FPointSample& Sample = Samples[Index];
				OutTransforms[Index].SetLocation(Sample.Location);

				//if (SourcesRange == 1)
				//{
				// TODO : Implement proper blending. Division by zero here when there are collocated points
				constexpr double Weight = 0.5; //FVector::DistSquared(Path->GetPos(Sample.Start), Sample.Location) / FVector::DistSquared(Path->GetPos(Sample.Start), Path->GetPos(Sample.End));
				MetadataBlender->Blend(Sample.Start, Sample.End, Index, Weight);
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
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

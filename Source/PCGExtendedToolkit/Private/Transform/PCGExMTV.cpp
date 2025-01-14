// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExMTV.h"


#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExMTVElement"
#define PCGEX_NAMESPACE MTV

PCGExData::EIOInit UPCGExMTVSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(MTV)

bool FPCGExMTVElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MTV)

	return true;
}

bool FPCGExMTVElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMTVElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MTV)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExMTV::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExMTV::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to relax."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExMTV
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMTV::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Iterations = Settings->MaxIterations;
		NumPoints = PointDataFacade->GetNum();
		Forces.Init(FIntVector3(0), NumPoints);

		InfluenceDetails = Settings->InfluenceDetails;
		if (!InfluenceDetails.Init(ExecutionContext, PointDataFacade)) { return false; }

		StartParallelLoopForPoints(PCGExData::ESource::Out, 32);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const FBox CurrentBox = Point.GetLocalBounds().TransformBy(Point.Transform);
		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetMutablePoints();

		bool bHasOverlap = false;

		for (int32 OtherIndex = Index + 1; OtherIndex < NumPoints; OtherIndex++)
		{
			const FPCGPoint& OtherPoint = InPoints[OtherIndex];

			const FBox OtherBox = OtherPoint.GetLocalBounds().TransformBy(OtherPoint.Transform);

			if (CurrentBox.Intersect(OtherBox))
			{
				FVector Delta = OtherPoint.Transform.GetLocation() - Point.Transform.GetLocation();
				const FVector Overlap = CurrentBox.GetExtent() + OtherBox.GetExtent() - PCGExMath::Abs(Delta);

				if (Overlap.X > 0 && Overlap.Y > 0 && Overlap.Z > 0)
				{
					double MinOverlap = Overlap.X;
					FVector MTV = FVector(Delta.X > 0 ? Settings->StepScale : -Settings->StepScale, 0, 0);

					if (Overlap.Y < MinOverlap)
					{
						MinOverlap = Overlap.Y;
						MTV = FVector(0, Delta.Y > 0 ? Settings->StepScale : -Settings->StepScale, 0);
					}

					if (Overlap.Z < MinOverlap)
					{
						MinOverlap = Overlap.Z;
						MTV = FVector(0, 0, Delta.Z > 0 ? Settings->StepScale : -Settings->StepScale);
					}

					const FVector Adjustment = (MTV * MinOverlap * 0.5) / 0.01;

					FPlatformAtomics::InterlockedAdd(&Forces[Index].X, -Adjustment.X);
					FPlatformAtomics::InterlockedAdd(&Forces[Index].Y, -Adjustment.Y);
					FPlatformAtomics::InterlockedAdd(&Forces[Index].Z, -Adjustment.Z);

					FPlatformAtomics::InterlockedAdd(&Forces[OtherIndex].X, Adjustment.X);
					FPlatformAtomics::InterlockedAdd(&Forces[OtherIndex].Y, Adjustment.Y);
					FPlatformAtomics::InterlockedAdd(&Forces[OtherIndex].Z, Adjustment.Z);

					bHasOverlap = true;
				}
			}
		}

		if (bHasOverlap) { FPlatformAtomics::InterlockedExchange(&bFoundOverlap, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		TArray<FPCGPoint>& OutPoints = PointDataFacade->GetMutablePoints();
		for (int i = 0; i < NumPoints; i++)
		{
			FIntVector3 F = Forces[i];
			OutPoints[i].Transform.AddToTranslation(FVector(F.X, F.Y, F.Z) * 0.01);
		}

		if (!bFoundOverlap) { return; } // Early exit, no more overlap to process

		bFoundOverlap = 0;
		Forces.Reset(NumPoints);
		Forces.Init(FIntVector3(0), NumPoints);

		Iterations--;
		if (Iterations <= 0) { return; }

		StartParallelLoopForPoints(PCGExData::ESource::Out, 32);
	}

	void FProcessor::CompleteWork()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

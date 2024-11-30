// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBoundsPathIntersection.h"
#include "PCGExRandom.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsPathIntersectionElement"
#define PCGEX_NAMESPACE BoundsPathIntersection

TArray<FPCGPinProperties> UPCGExBoundsPathIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "Intersection points (bounds)", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExBoundsPathIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(BoundsPathIntersection)

bool FPCGExBoundsPathIntersectionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsPathIntersection)

	if (!Settings->OutputSettings.Validate(Context)) { return false; }

	Context->BoundsDataFacade = PCGExData::TryGetSingleFacade(InContext, PCGEx::SourceBoundsLabel, true);
	if (!Context->BoundsDataFacade) { return false; }

	return true;
}

bool FPCGExBoundsPathIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsPathIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsPathIntersection)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		const bool bWritesAny = Settings->OutputSettings.WillWriteAny();
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathIntersections::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitInvalidPathsOutputs)
					{
						if (bWritesAny)
						{
							Entry->InitializeOutput(PCGExData::EIOInit::Duplicate);
							Settings->OutputSettings.Mark(Entry.ToSharedRef());
						}
						else
						{
							Entry->InitializeOutput(PCGExData::EIOInit::Forward);
						}
					}

					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathIntersections::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = Settings->OutputSettings.WillWriteAny();
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to intersect with."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathIntersections
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathIntersections::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		LastIndex = PointDataFacade->GetNum() - 1;
		Segmentation = MakeShared<PCGExGeo::FSegmentation>();
		Cloud = Context->BoundsDataFacade->GetCloud(Settings->OutputSettings.BoundsSource, Settings->OutputSettings.InsideEpsilon);

		Details = Settings->OutputSettings;

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FindIntersectionsTaskGroup)

		TWeakPtr<FProcessor> WeakThisPtr = SharedThis(this);

		FindIntersectionsTaskGroup->OnSubLoopStartCallback =
			[WeakThisPtr](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedPtr<FProcessor> This = WeakThisPtr.Pin();
				if (!This) { return; }

				This->PointDataFacade->Fetch(StartIndex, Count);
				This->FilterScope(StartIndex, Count);

				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++) { This->FindIntersections(i); }
			};

		FindIntersectionsTaskGroup->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::FindIntersections(const int32 Index) const
	{
		int32 NextIndex = Index + 1;

		if (Index == LastIndex)
		{
			if (bClosedLoop) { NextIndex = 0; }
			else { return; }
		}

		const FVector StartPosition = PointDataFacade->Source->GetInPoint(Index).Transform.GetLocation();
		const FVector EndPosition = PointDataFacade->Source->GetInPoint(NextIndex).Transform.GetLocation();

		const TSharedPtr<PCGExGeo::FIntersections> Intersections = MakeShared<PCGExGeo::FIntersections>(
			StartPosition, EndPosition, Index, NextIndex);

		if (Cloud->FindIntersections(Intersections.Get()))
		{
			Intersections->SortAndDedupe();
			Segmentation->Insert(Intersections);
		}
	}

	void FProcessor::InsertIntersections(const int32 Index) const
	{
		const TSharedPtr<PCGExGeo::FIntersections> Intersections = Segmentation->IntersectionsList[Index];
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
		for (int i = 0; i < Intersections->Cuts.Num(); i++)
		{
			const int32 Idx = Intersections->Start + i;

			FPCGPoint& Point = MutablePoints[Idx];
			PCGExGeo::FCut& Cut = Intersections->Cuts[i];
			Point.Transform.SetLocation(Cut.Position);

			Point.Seed = PCGExRandom::ComputeSeed(Point);

			Details.SetIntersection(Idx, Cut.Normal, Cut.BoxIndex);
		}
	}

	void FProcessor::OnInsertionComplete()
	{
		if (!Details.IsInsideWriter && !Details.InsideForwardHandler) { return; }
		StartParallelLoopForPoints();
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumCuts = Segmentation->GetNumCuts();
		if (NumCuts == 0)
		{
			if (Settings->OutputSettings.WillWriteAny())
			{
				PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Duplicate);

				Details.Mark(PointDataFacade->Source);
				Details.Init(PointDataFacade, Context->BoundsDataFacade);

				StartParallelLoopForPoints();
			}
			else
			{
				PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
			}

			return;
		}

		PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::New);
		const TArray<FPCGPoint>& OriginalPoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		PCGEx::InitArray(MutablePoints, OriginalPoints.Num() + NumCuts);

		UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;

		int32 Idx = 0;

		for (int i = 0; i < LastIndex; i++)
		{
			const FPCGPoint& OriginalPoint = OriginalPoints[i];
			MutablePoints[Idx++] = OriginalPoint;

			if (const TSharedPtr<PCGExGeo::FIntersections> Intersections = Segmentation->Find(PCGEx::H64U(i, i + 1)))
			{
				Intersections->Start = Idx;
				for (int j = 0; j < Intersections->Cuts.Num(); j++)
				{
					FPCGPoint& NewPoint = MutablePoints[Idx++] = OriginalPoint;
					NewPoint.MetadataEntry = PCGInvalidEntryKey;
					Metadata->InitializeOnSet(NewPoint.MetadataEntry);
				}
			}
		}

		const FPCGPoint& OriginalPoint = OriginalPoints[LastIndex];
		MutablePoints[Idx++] = OriginalPoint;

		if (bClosedLoop)
		{
			if (const TSharedPtr<PCGExGeo::FIntersections> Intersections = Segmentation->Find(PCGEx::H64U(LastIndex, 0)))
			{
				Intersections->Start = Idx;
				for (int j = 0; j < Intersections->Cuts.Num(); j++)
				{
					FPCGPoint& NewPoint = MutablePoints[Idx++] = OriginalPoint;
					NewPoint.MetadataEntry = PCGInvalidEntryKey;
					Metadata->InitializeOnSet(NewPoint.MetadataEntry);
				}
			}
		}

		PointDataFacade->Source->CleanupKeys();
		Details.Init(PointDataFacade, Context->BoundsDataFacade);

		Segmentation->ReduceToArray();

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, InsertionTaskGroup)

		TWeakPtr<FProcessor> WeakThisPtr = SharedThis(this);

		InsertionTaskGroup->OnCompleteCallback =
			[WeakThisPtr]() { if (const TSharedPtr<FProcessor> This = WeakThisPtr.Pin()) { This->OnInsertionComplete(); } };

		InsertionTaskGroup->OnSubLoopStartCallback =
			[WeakThisPtr](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedPtr<FProcessor> This = WeakThisPtr.Pin();
				if (!This) { return; }

				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++) { This->InsertIntersections(i); }
			};

		InsertionTaskGroup->StartSubLoops(Segmentation->IntersectionsList.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		FPointsProcessor::CompleteWork();
	}

	void FProcessor::Write()
	{
		FPointsProcessor::Write();
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

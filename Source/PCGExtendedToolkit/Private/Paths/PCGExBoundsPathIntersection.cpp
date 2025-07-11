// Copyright 2025 Timothé Lapetite and contributors
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

PCGEX_INITIALIZE_ELEMENT(BoundsPathIntersection)

bool FPCGExBoundsPathIntersectionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsPathIntersection)

	if (!Settings->OutputSettings.Validate(Context)) { return false; }

	Context->BoundsDataFacade = PCGExData::TryGetSingleFacade(InContext, PCGEx::SourceBoundsLabel, false, true);
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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathIntersections::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IPointsProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());
		LastIndex = PointDataFacade->GetNum() - 1;
		Segmentation = MakeShared<PCGExGeo::FSegmentation>();
		Cloud = Context->BoundsDataFacade->GetCloud(Settings->OutputSettings.BoundsSource, Settings->OutputSettings.InsideExpansion);

		Details = Settings->OutputSettings;

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FindIntersectionsTaskGroup)

		FindIntersectionsTaskGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);
				This->FilterScope(Scope);

				PCGEX_SCOPE_LOOP(i) { This->FindIntersections(i); }
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

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->Source->GetIn()->GetConstTransformValueRange();

		const FVector StartPosition = InTransforms[Index].GetLocation();
		const FVector EndPosition = InTransforms[NextIndex].GetLocation();

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

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->Source->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<int32> OutSeeds = PointDataFacade->Source->GetOut()->GetSeedValueRange(false);

		for (int i = 0; i < Intersections->Cuts.Num(); i++)
		{
			const int32 Idx = Intersections->Start + i;

			PCGExGeo::FCut& Cut = Intersections->Cuts[i];
			OutTransforms[Idx].SetLocation(Cut.Position);

			OutSeeds[Idx] = PCGExRandom::ComputeSpatialSeed(Cut.Position);

			Details.SetIntersection(Idx, Cut.Normal, Cut.BoxIndex);
		}
	}

	void FProcessor::OnInsertionComplete()
	{
		if (!Details.IsInsideWriter && !Details.InsideForwardHandler) { return; }
		StartParallelLoopForPoints();
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BoundsPathIntersection::ProcessPoints);

		TConstPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			if (Details.InsideForwardHandler)
			{
				TArray<TSharedPtr<PCGExGeo::FPointBox>> Overlaps;
				const bool bContained = Cloud->IsInside<EPCGExBoxCheckMode::ExpandedBox>(OutTransforms[Index].GetLocation(), Overlaps); // Avoid intersections being captured
				Details.SetIsInside(Index, bContained, bContained ? Overlaps[0]->Index : -1);
			}
			else
			{
				Details.SetIsInside(Index, Cloud->IsInside<EPCGExBoxCheckMode::ExpandedBox>(OutTransforms[Index].GetLocation()));
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumCuts = Segmentation->GetNumCuts();
		if (NumCuts == 0)
		{
			if (Settings->OutputSettings.WillWriteAny())
			{
				PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

				Details.Mark(PointDataFacade->Source);
				Details.Init(PointDataFacade, Context->BoundsDataFacade);

				StartParallelLoopForPoints();
			}
			else
			{
				PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Forward)
			}

			return;
		}

		PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New)

		const UPCGBasePointData* OriginalPoints = PointDataFacade->GetIn();
		UPCGBasePointData* MutablePoints = PointDataFacade->GetOut();
		PCGEx::SetNumPointsAllocated(MutablePoints, OriginalPoints->GetNumPoints() + NumCuts, OriginalPoints->GetAllocatedProperties());

		TArray<int32>& IdxMapping = PointDataFacade->Source->GetIdxMapping();

		TConstPCGValueRange<int64> InMetadataEntries = OriginalPoints->GetConstMetadataEntryValueRange();
		TPCGValueRange<int64> OutMetadataEntries = MutablePoints->GetMetadataEntryValueRange();

		UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;

		int32 Idx = 0;

		for (int i = 0; i < LastIndex; i++)
		{
			OutMetadataEntries[Idx] = InMetadataEntries[i];
			IdxMapping[Idx++] = i;

			if (const TSharedPtr<PCGExGeo::FIntersections> Intersections = Segmentation->Find(PCGEx::H64U(i, i + 1)))
			{
				Intersections->Start = Idx;
				for (int j = 0; j < Intersections->Cuts.Num(); j++)
				{
					OutMetadataEntries[Idx] = PCGInvalidEntryKey;
					Metadata->InitializeOnSet(OutMetadataEntries[Idx]);

					IdxMapping[Idx++] = i;
				}
			}
		}

		OutMetadataEntries[Idx] = InMetadataEntries[LastIndex];
		IdxMapping[Idx++] = LastIndex;


		// TODO : Inherit properties, but metadata

		if (bClosedLoop)
		{
			if (const TSharedPtr<PCGExGeo::FIntersections> Intersections = Segmentation->Find(PCGEx::H64U(LastIndex, 0)))
			{
				Intersections->Start = Idx;
				for (int j = 0; j < Intersections->Cuts.Num(); j++)
				{
					OutMetadataEntries[Idx] = PCGInvalidEntryKey;
					Metadata->InitializeOnSet(OutMetadataEntries[Idx]);

					IdxMapping[Idx++] = LastIndex;
				}
			}
		}

		// Copy point properties, we'll do blending & inheriting right after
		// At this point we want to preserve metadata entries
		PointDataFacade->Source->ConsumeIdxMapping(PCGEx::AllPointNativePropertiesButMeta);

		PointDataFacade->Source->ClearCachedKeys();
		Details.Init(PointDataFacade, Context->BoundsDataFacade);

		Segmentation->ReduceToArray();

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, InsertionTaskGroup)

		InsertionTaskGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnInsertionComplete();
			};

		InsertionTaskGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				PCGEX_SCOPE_LOOP(i) { This->InsertIntersections(i); }
			};

		InsertionTaskGroup->StartSubLoops(Segmentation->IntersectionsList.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		IPointsProcessor::CompleteWork();
	}

	void FProcessor::Write()
	{
		IPointsProcessor::Write();
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

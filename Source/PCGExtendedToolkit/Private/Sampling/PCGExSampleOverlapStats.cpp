﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleOverlapStats.h"

#include "PCGExMathBounds.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExSampleOverlapStatsElement"
#define PCGEX_NAMESPACE SampleOverlapStats

TSharedPtr<PCGExSampleOverlapStats::FOverlap> FPCGExSampleOverlapStatsContext::RegisterOverlap(
	PCGExSampleOverlapStats::FProcessor* InA,
	PCGExSampleOverlapStats::FProcessor* InB,
	const FBox& InIntersection)
{
	const uint64 HashID = PCGEx::H64U(InA->BatchIndex, InB->BatchIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (TSharedPtr<PCGExSampleOverlapStats::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }
	}

	{
		FWriteScopeLock WriteScopeLock(OverlapLock);
		if (TSharedPtr<PCGExSampleOverlapStats::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }

		PCGEX_MAKE_SHARED(NewOverlap, PCGExSampleOverlapStats::FOverlap, InA, InB, InIntersection)
		OverlapMap.Add(HashID, NewOverlap);
		return NewOverlap;
	}
}

void FPCGExSampleOverlapStatsContext::BatchProcessing_WorkComplete()
{
	FPCGExPointsProcessorContext::BatchProcessing_WorkComplete();

	const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>> TypedBatch = StaticCastSharedPtr<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>>(MainBatch);

	for (int Pi = 0; Pi < TypedBatch->GetNumProcessors(); Pi++)
	{
		const TSharedPtr<PCGExSampleOverlapStats::FProcessor> P = TypedBatch->GetProcessor<PCGExSampleOverlapStats::FProcessor>(Pi);
		if (!P->bIsProcessorValid) { continue; }
		SharedOverlapSubCountMax = FMath::Max(SharedOverlapSubCountMax, P->LocalOverlapSubCountMax);
		SharedOverlapCountMax = FMath::Max(SharedOverlapCountMax, P->LocalOverlapCountMax);
	}
}

PCGEX_INITIALIZE_ELEMENT(SampleOverlapStats)

PCGExData::EIOInit UPCGExSampleOverlapStatsSettings::GetIOPreInitForMainPoints() const{ return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleOverlapStats)

bool FPCGExSampleOverlapStatsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }


	PCGEX_CONTEXT_AND_SETTINGS(SampleOverlapStats)

	PCGEX_FOREACH_FIELD_SAMPLEOVERLAPSTATS(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Context->MainPoints->Num() < 2)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Not enough inputs; requires at least 2 to check for overlap."));
		return false;
	}

	return true;
}

bool FPCGExSampleOverlapStatsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleOverlapStatsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleOverlapStats)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any input to check for overlaps."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleOverlapStats
{
	FOverlap::FOverlap(FProcessor* InPrimary, FProcessor* InSecondary, const FBox& InIntersection):
		Intersection(InIntersection), Primary(InPrimary), Secondary(InSecondary)
	{
		HashID = PCGEx::H64U(InPrimary->BatchIndex, InSecondary->BatchIndex);
	}

	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterOverlap(FProcessor* InOtherProcessor, const FBox& Intersection)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		const TSharedPtr<FOverlap> Overlap = Context->RegisterOverlap(this, InOtherProcessor, Intersection);
		if (Overlap->Primary == this) { ManagedOverlaps.Add(Overlap.ToSharedRef()); }
		Overlaps.Add(Overlap.ToSharedRef());
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SAMPLEOVERLAPSTATS(PCGEX_OUTPUT_INIT)
		}


		// 1 - Build bounds & octrees

		InPoints = PointDataFacade->GetIn();
		NumPoints = InPoints->GetNumPoints();

		LocalPointBounds.Init(nullptr, NumPoints);
		OverlapSubCount.Init(0, NumPoints);
		OverlapCount.Init(0, NumPoints);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, BoundsPreparationTask)

		BoundsPreparationTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS

				This->Octree = MakeUnique<PCGExDiscardByOverlap::FPointBoundsOctree>(This->Bounds.GetCenter(), This->Bounds.GetExtent().Length());
				for (const TSharedPtr<PCGExDiscardByOverlap::FPointBounds>& PtBounds : This->LocalPointBounds)
				{
					if (!PtBounds) { continue; }
					This->Octree->AddElement(PtBounds.Get());
				}
			};


		BoundsPreparationTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);
				This->FilterScope(Scope);

#define PCGEX_POINT_CHECK if (!This->PointFilterCache[i]) { continue; } const PCGExData::FConstPoint Point(This->InPoints, i);

				if (This->Settings->BoundsSource == EPCGExPointBoundsSource::ScaledBounds)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						PCGEX_POINT_CHECK

						const FBox LocalBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).ExpandBy(This->Settings->Expansion);
						TSharedPtr<PCGExDiscardByOverlap::FPointBounds> PtBounds = MakeShared<PCGExDiscardByOverlap::FPointBounds>(i, Point, LocalBounds);
						This->RegisterPointBounds(i, PtBounds);
					}
				}
				else if (This->Settings->BoundsSource == EPCGExPointBoundsSource::DensityBounds)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						PCGEX_POINT_CHECK

						const FBox LocalBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point).ExpandBy(This->Settings->Expansion);
						TSharedPtr<PCGExDiscardByOverlap::FPointBounds> PtBounds = MakeShared<PCGExDiscardByOverlap::FPointBounds>(i, Point, LocalBounds);
						This->RegisterPointBounds(i, PtBounds);
					}
				}
				else if (This->Settings->BoundsSource == EPCGExPointBoundsSource::Bounds)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						PCGEX_POINT_CHECK
						const FBox LocalBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point).ExpandBy(This->Settings->Expansion);
						TSharedPtr<PCGExDiscardByOverlap::FPointBounds> PtBounds = MakeShared<PCGExDiscardByOverlap::FPointBounds>(i, Point, LocalBounds);
						This->RegisterPointBounds(i, PtBounds);
					}
				}
				else if (This->Settings->BoundsSource == EPCGExPointBoundsSource::Center)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						PCGEX_POINT_CHECK
						const FBox LocalBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Center>(Point).ExpandBy(This->Settings->Expansion);
						TSharedPtr<PCGExDiscardByOverlap::FPointBounds> PtBounds = MakeShared<PCGExDiscardByOverlap::FPointBounds>(i, Point, LocalBounds);
						This->RegisterPointBounds(i, PtBounds);
					}
				}
			};

		BoundsPreparationTask->StartSubLoops(NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);

		return true;
	}

	void FProcessor::ResolveOverlap(const int32 Index)
	{
		// For each managed overlap, find per-point intersections

		const TSharedPtr<FOverlap> Overlap = Overlaps[Index];
		const bool bUpdateOverlap = ManagedOverlaps.Contains(Overlap);
		const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*ParentBatch.Pin()->SubProcessorMap->Find(&Overlap->GetOther(this)->PointDataFacade->Source.Get()));

		TConstPCGValueRange<FTransform> InTransforms = InPoints->GetConstTransformValueRange();

		if (Settings->TestMode != EPCGExOverlapTestMode::Sphere)
		{
			Octree->FindElementsWithBoundsTest(
				FBoxCenterAndExtent(Overlap->Intersection.GetCenter(), Overlap->Intersection.GetExtent()),
				[&](const PCGExDiscardByOverlap::FPointBounds* OwnedPoint)
				{
					const double Length = OwnedPoint->LocalBounds.GetExtent().Length() * 2;
					const FMatrix InvMatrix = InTransforms[OwnedPoint->Index].ToMatrixNoScale().Inverse();

					int32 Count = 0;

					OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(OwnedPoint->Bounds.GetBox()), [&](const PCGExDiscardByOverlap::FPointBounds* OtherPoint)
						{
							const FBox Intersection = OwnedPoint->LocalBounds.Overlap(OtherPoint->TransposedBounds(InvMatrix));

							if (!Intersection.IsValid) { return; }

							const double OverlapSize = Intersection.GetExtent().Length() * 2;
							if (Settings->ThresholdMeasure == EPCGExMeanMeasure::Relative) { if ((OverlapSize / Length) < Settings->MinThreshold) { return; } }
							else if (OverlapSize < Settings->MinThreshold) { return; }

							Count++;

							if (bUpdateOverlap)
							{
								Overlap->Stats.OverlapCount++;
								Overlap->Stats.OverlapVolume += Intersection.GetVolume();
							}
						});

					if (Count > 0)
					{
						FPlatformAtomics::InterlockedExchange(&bAnyOverlap, 1);
						FPlatformAtomics::InterlockedAdd(&OverlapSubCount[OwnedPoint->Index], Count);
						FPlatformAtomics::InterlockedAdd(&OverlapCount[OwnedPoint->Index], 1);
					}
				});
		}
		else
		{
			Octree->FindElementsWithBoundsTest(
				FBoxCenterAndExtent(Overlap->Intersection.GetCenter(), Overlap->Intersection.GetExtent()),
				[&](const PCGExDiscardByOverlap::FPointBounds* OwnedPoint)
				{
					const FSphere S1 = OwnedPoint->Bounds.GetSphere();

					int32 Count = 0;

					OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(OwnedPoint->Bounds.GetBox()), [&](const PCGExDiscardByOverlap::FPointBounds* OtherPoint)
						{
							double OverlapAmount = 0;
							if (!PCGExMath::SphereOverlap(S1, OtherPoint->Bounds.GetSphere(), OverlapAmount)) { return; }

							if (Settings->ThresholdMeasure == EPCGExMeanMeasure::Relative) { if ((OverlapAmount / S1.W) < Settings->MinThreshold) { return; } }
							else if (OverlapAmount < Settings->MinThreshold) { return; }

							Count++;

							if (bUpdateOverlap)
							{
								Overlap->Stats.OverlapCount++;
								Overlap->Stats.OverlapVolume += OverlapAmount;
							}
						});

					if (Count > 0)
					{
						FPlatformAtomics::InterlockedExchange(&bAnyOverlap, 1);
						FPlatformAtomics::InterlockedAdd(&OverlapSubCount[OwnedPoint->Index], Count);
						FPlatformAtomics::InterlockedAdd(&OverlapCount[OwnedPoint->Index], 1);
					}
				});
		}
	}

	void FProcessor::WriteSingleData(const int32 Index)
	{
		const int32 TOC = OverlapSubCount[Index];
		const int32 UOC = OverlapCount[Index];

		PCGEX_OUTPUT_VALUE(OverlapSubCount, Index, TOC)
		PCGEX_OUTPUT_VALUE(OverlapCount, Index, UOC)
		PCGEX_OUTPUT_VALUE(RelativeOverlapSubCount, Index, static_cast<double>(TOC) / Context->SharedOverlapSubCountMax)
		PCGEX_OUTPUT_VALUE(RelativeOverlapCount, Index, static_cast<double>(UOC) / Context->SharedOverlapCountMax)
	}

	void FProcessor::CompleteWork()
	{
		// 2 - Find overlaps between large bounds, we'll be searching only there.

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PreparationTask)
		PreparationTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				auto WrapUp = [AsyncThis]()
				{
					PCGEX_ASYNC_NESTED_THIS
					for (int i = 0; i < NestedThis->NumPoints; i++)
					{
						NestedThis->LocalOverlapSubCountMax = FMath::Max(NestedThis->LocalOverlapSubCountMax, NestedThis->OverlapSubCount[i]);
						NestedThis->LocalOverlapCountMax = FMath::Max(NestedThis->LocalOverlapCountMax, NestedThis->OverlapCount[i]);
					}
				};

				if (This->Overlaps.IsEmpty())
				{
					WrapUp();
					return;
				}

				PCGEX_ASYNC_GROUP_CHKD_VOID(This->AsyncManager, SearchTask)
				SearchTask->OnCompleteCallback = WrapUp;
				SearchTask->OnSubLoopStartCallback = [AsyncThis](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_NESTED_THIS
					PCGEX_SCOPE_LOOP(i) { NestedThis->ResolveOverlap(i); }
				};
				SearchTask->StartSubLoops(This->Overlaps.Num(), 8);
			};

		PreparationTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				const TSharedPtr<PCGExPointsMT::IBatch> Parent = This->ParentBatch.Pin();
				PCGEX_SCOPE_LOOP(i)
				{
					const TSharedPtr<PCGExData::FFacade> OtherFacade = Parent->ProcessorFacades[i];
					if (This->PointDataFacade == OtherFacade) { continue; } // Skip self

					const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*Parent->SubProcessorMap->Find(&OtherFacade->Source.Get()));

					const FBox Intersection = This->Bounds.Overlap(OtherProcessor->GetBounds());
					if (!Intersection.IsValid) { continue; } // No overlap

					This->RegisterOverlap(&OtherProcessor.Get(), Intersection);
				}
			};
		PreparationTask->StartSubLoops(ParentBatch.Pin()->ProcessorFacades.Num(), 64);
	}

	void FProcessor::Write()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SearchTask)

		SearchTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->WriteFastest(This->AsyncManager);
				if (This->Settings->bTagIfHasAnyOverlap && This->bAnyOverlap) { This->PointDataFacade->Source->Tags->AddRaw(This->Settings->HasAnyOverlapTag); }
				if (This->Settings->bTagIfHasNoOverlap && !This->bAnyOverlap) { This->PointDataFacade->Source->Tags->AddRaw(This->Settings->HasNoOverlapTag); }
			};

		SearchTask->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->WriteSingleData(Index);
			};

		SearchTask->StartIterations(NumPoints, ParentBatch.Pin()->ProcessorFacades.Num());
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

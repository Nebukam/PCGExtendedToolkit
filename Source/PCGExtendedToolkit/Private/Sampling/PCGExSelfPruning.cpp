﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSelfPruning.h"

#include "PCGExRandom.h"
#include "PCGExSorting.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGPointData.h"
#include "Details/PCGExDetailsSettings.h"

#define LOCTEXT_NAMESPACE "PCGExSelfPruningElement"
#define PCGEX_NAMESPACE SelfPruning

PCGEX_SETTING_VALUE_IMPL(UPCGExSelfPruningSettings, PrimaryExpansion, double, PrimaryExpansionInput, PrimaryExpansionAttribute, PrimaryExpansion)
PCGEX_SETTING_VALUE_IMPL(UPCGExSelfPruningSettings, SecondaryExpansion, double, SecondaryExpansionInput, SecondaryExpansionAttribute, SecondaryExpansion)

#if WITH_EDITOR
bool UPCGExSelfPruningSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if ((Mode != EPCGExSelfPruningMode::Prune || bRandomize)
		&& InPin->Properties.Label == PCGExSorting::SourceSortingRules) { return false; }

	return Super::IsPinUsedByNodeExecution(InPin);
}
#endif

bool UPCGExSelfPruningSettings::HasDynamicPins() const
{
	return IsInputless();
}

TArray<FPCGPinProperties> UPCGExSelfPruningSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, bRandomize ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal);
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SelfPruning)
PCGEX_ELEMENT_BATCH_POINT_IMPL(SelfPruning)

bool FPCGExSelfPruningElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SelfPruning)

	PCGEX_VALIDATE_NAME_CONDITIONAL(Settings->Mode == EPCGExSelfPruningMode::WriteResult, Settings->NumOverlapAttributeName)

	return true;
}

bool FPCGExSelfPruningElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSelfPruningElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SelfPruning)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();
	Context->Done();


	return Context->TryComplete();
}


namespace PCGExSelfPruning
{

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSelfPruning::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->PrimaryMode != EPCGExSelfPruningExpandOrder::None)
		{
			PrimaryExpansion = Settings->GetValueSettingPrimaryExpansion();
			if (!PrimaryExpansion->Init(PointDataFacade)) { return false; }
		}

		if (Settings->SecondaryMode != EPCGExSelfPruningExpandOrder::None)
		{
			SecondaryExpansion = Settings->GetValueSettingSecondaryExpansion();
			if (!SecondaryExpansion->Init(PointDataFacade)) { return false; }
		}

		const int32 NumPoints = PointDataFacade->GetNum();

		if (Settings->Mode == EPCGExSelfPruningMode::WriteResult) { PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate) }
		else { Mask.Init(true, NumPoints); }

		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumPoints);

		PCGEx::InitArray(Candidates, NumPoints);
		Priority.SetNumUninitialized(NumPoints);
		BoxSecondary.Init(FBox(NoInit), NumPoints);

		if (Settings->Mode == EPCGExSelfPruningMode::Prune)
		{
			TSharedPtr<PCGExSorting::FPointSorter> Sorter = MakeShared<PCGExSorting::FPointSorter>(Context, PointDataFacade, PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
			Sorter->SortDirection = Settings->SortDirection;
			if (Sorter->Init(Context)) { Order.Sort([&](const int32 A, const int32 B) { return Sorter->Sort(A, B); }); }

			if (Settings->bRandomize)
			{
				TConstPCGValueRange<int32> Seeds = PointDataFacade->GetIn()->GetConstSeedValueRange();
				const int32 MaxRange = NumPoints * Settings->RandomRange;
				const int32 MinRange = -MaxRange;
				for (int i = 0; i < NumPoints; i++) { Priority[i] = Order[i] + PCGExRandom::GetRandomStreamFromPoint(Seeds[i], 0, Settings).RandRange(MinRange, MaxRange); }
				if (Settings->SortDirection == EPCGExSortDirection::Descending) { Order.Sort([&](const int32 A, const int32 B) { return Priority[A] > Priority[B]; }); }
				else { Order.Sort([&](const int32 A, const int32 B) { return Priority[A] < Priority[B]; }); }
			}
		}

		for (int32 i = 0; i < NumPoints; i++) { Priority[Order[i]] = i; }

		bDaisyChainProcessRange = Settings->Mode == EPCGExSelfPruningMode::Prune;
		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		const UPCGBasePointData* InData = PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> Transforms = InData->GetConstTransformValueRange();

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			FCandidateInfos& Candidate = Candidates[Index];
			Candidate.Index = Index;
			Candidate.bSkip = false;
			Candidate.Overlaps = 0;
		}

		switch (Settings->SecondaryMode)
		{
		case EPCGExSelfPruningExpandOrder::Before:
			PCGEX_SCOPE_LOOP(Index) { BoxSecondary[Index] = InData->GetLocalBounds(Index).ExpandBy(SecondaryExpansion->Read(Index)).TransformBy(Transforms[Index]); }
			break;
		case EPCGExSelfPruningExpandOrder::After:
			PCGEX_SCOPE_LOOP(Index) { BoxSecondary[Index] = InData->GetLocalBounds(Index).TransformBy(Transforms[Index]).ExpandBy(SecondaryExpansion->Read(Index)); }
			break;
		default:
		case EPCGExSelfPruningExpandOrder::None:
			PCGEX_SCOPE_LOOP(Index) { BoxSecondary[Index] = InData->GetLocalBounds(Index).TransformBy(Transforms[Index]); }
			break;
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		Candidates.Sort([&](const FCandidateInfos& A, const FCandidateInfos& B) { return Priority[A.Index] > Priority[B.Index]; });
		LastCandidatesCount = Candidates.Num();

		StartParallelLoopForRange(Candidates.Num());
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SelfPruning::ProcessRange);

		const UPCGBasePointData* InData = PointDataFacade->GetIn();
		const PCGPointOctree::FPointOctree& Octree = InData->GetPointOctree();
		TConstPCGValueRange<FTransform> Transforms = InData->GetConstTransformValueRange();

		if (Settings->Mode == EPCGExSelfPruningMode::WriteResult)
		{
			PCGEX_SCOPE_LOOP(i)
			{
				FCandidateInfos& Candidate = Candidates[i];
				Candidate.bSkip = true;

				const int32 Index = Candidate.Index;
				const FTransform& Transform = Transforms[Index];

				FBox BoxA = InData->GetLocalBounds(Index);
				FBox Box = FBox(NoInit);
				if (Settings->PrimaryMode == EPCGExSelfPruningExpandOrder::Before)
				{
					BoxA = BoxA.ExpandBy(PrimaryExpansion->Read(Index));
					Box = BoxA.TransformBy(Transform);
				}
				else if (Settings->PrimaryMode == EPCGExSelfPruningExpandOrder::After)
				{
					Box = BoxA.TransformBy(Transform).ExpandBy(PrimaryExpansion->Read(Index));
					BoxA = BoxA.ExpandBy(PrimaryExpansion->Read(Index));
				}
				else
				{
					Box = BoxA.TransformBy(Transform);
				}

				Octree.FindElementsWithBoundsTest(
					Box, [&](const PCGPointOctree::FPointRef& Other)
					{
						const int32 OtherIndex = Other.Index;

						// Ignore self
						if (OtherIndex == Index || !PointFilterCache[OtherIndex]) { return; }
						if (Box.Intersect(BoxSecondary[Other.Index]))
						{
							if (Settings->bPreciseTest)
							{
								FBox BoxB = InData->GetLocalBounds(OtherIndex);
								if (Settings->SecondaryMode == EPCGExSelfPruningExpandOrder::Before) { BoxB = BoxB.ExpandBy(SecondaryExpansion->Read(OtherIndex)); }
								else if (Settings->SecondaryMode == EPCGExSelfPruningExpandOrder::After) { BoxB = BoxB.ExpandBy(SecondaryExpansion->Read(OtherIndex)); }

								if (!PCGExGeo::IntersectOBB_OBB(BoxA, Transform, BoxB, Transforms[OtherIndex])) { return; }
							}

							Candidate.Overlaps++;
						}
					});
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(i)
			{
				FCandidateInfos& Candidate = Candidates[i];
				Candidate.bSkip = true;

				const int32 Index = Candidate.Index;

				if (!PointFilterCache[Index]) { continue; }

				const int32 CurrentPriority = Priority[Index];
				const FTransform& Transform = Transforms[Index];


				FBox BoxA = InData->GetLocalBounds(Index);
				FBox Box = FBox(NoInit);
				if (Settings->PrimaryMode == EPCGExSelfPruningExpandOrder::Before)
				{
					BoxA = BoxA.ExpandBy(PrimaryExpansion->Read(Index));
					Box = BoxA.TransformBy(Transform);
				}
				else if (Settings->PrimaryMode == EPCGExSelfPruningExpandOrder::After)
				{
					Box = BoxA.TransformBy(Transform).ExpandBy(PrimaryExpansion->Read(Index));
					BoxA = BoxA.ExpandBy(PrimaryExpansion->Read(Index));
				}
				else
				{
					Box = BoxA.TransformBy(Transform);
				}

				Octree.FindFirstElementWithBoundsTest(
					Box, [&](const PCGPointOctree::FPointRef& Other)
					{
						const int32 OtherIndex = Other.Index;

						// Ignore self
						if (OtherIndex == Index
							|| !PointFilterCache[OtherIndex]
							|| !Mask[OtherIndex]) { return true; }

						// Ignore lower priorities, those will be pruned by this candidate when their turn comes
						if (Priority[OtherIndex] < CurrentPriority) { return true; }

						if (Box.Intersect(BoxSecondary[OtherIndex]))
						{
							if (Settings->bPreciseTest)
							{
								FBox BoxB = InData->GetLocalBounds(OtherIndex);
								if (Settings->SecondaryMode == EPCGExSelfPruningExpandOrder::Before) { BoxB = BoxB.ExpandBy(SecondaryExpansion->Read(OtherIndex)); }
								else if (Settings->SecondaryMode == EPCGExSelfPruningExpandOrder::After) { BoxB = BoxB.ExpandBy(SecondaryExpansion->Read(OtherIndex)); }

								if (!PCGExGeo::IntersectOBB_OBB(BoxA, Transform, BoxB, Transforms[OtherIndex])) { return true; }
							}

							Mask[Index] = false;
							return false;
						}

						return true;
					});
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		if (Settings->Mode == EPCGExSelfPruningMode::WriteResult)
		{
			if (Settings->Units == EPCGExMeanMeasure::Relative)
			{
				TSharedPtr<PCGExData::TBuffer<double>> Buffer = PointDataFacade->GetWritable<double>(Settings->NumOverlapAttributeName, 0, true, PCGExData::EBufferInit::New);
				double Max = 0;
				for (int i = 0; i < Candidates.Num(); i++) { Max = FMath::Max(Max, Candidates[i].Overlaps); }

				if (Max == 0) { return; } // Shoo shoo divide by zero

				if (Settings->bOutputOneMinusOverlap)
				{
					for (const FCandidateInfos& Candidate : Candidates) { Buffer->SetValue(Candidate.Index, 1 - (static_cast<double>(Candidate.Overlaps) / Max)); }
				}
				else
				{
					for (const FCandidateInfos& Candidate : Candidates) { Buffer->SetValue(Candidate.Index, (static_cast<double>(Candidate.Overlaps) / Max)); }
				}
			}
			else
			{
				TSharedPtr<PCGExData::TBuffer<int32>> Buffer = PointDataFacade->GetWritable<int32>(Settings->NumOverlapAttributeName, 0, true, PCGExData::EBufferInit::New);
				for (const FCandidateInfos& Candidate : Candidates) { Buffer->SetValue(Candidate.Index, Candidate.Overlaps); }
			}

			return;
		}

		int32 WriteIndex = 0;
		for (int32 i = 0; i < Candidates.Num(); i++)
		{
			FCandidateInfos& Candidate = Candidates[i];
			if (!Candidate.bSkip) { Candidates[WriteIndex++] = MoveTemp(Candidate); }
		}

		Candidates.SetNum(WriteIndex);

		if (!Candidates.IsEmpty())
		{
			if (LastCandidatesCount == WriteIndex)
			{
				// Error
				// Somehow the last run could not get rid of some overlap, number of candidates hasn't changed
				// We have to stop iterating now or we'll iterate forever
				return;
			}

			LastCandidatesCount = WriteIndex;
			StartParallelLoopForRange(WriteIndex);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Mode == EPCGExSelfPruningMode::WriteResult)
		{
			PointDataFacade->WriteFastest(AsyncManager);
			return;
		}

		bool bAnyPruning = false;
		for (int32 i = 0; i < Mask.Num(); i++)
		{
			if (!Mask[i])
			{
				bAnyPruning = true;
				break;
			}
		}

		if (!bAnyPruning)
		{
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Forward)
			return;
		}

		PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		(void)PointDataFacade->Source->Gather(Mask);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

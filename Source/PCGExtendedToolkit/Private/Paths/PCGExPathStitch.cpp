// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathStitch.h"
#include "PCGExMath.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathStitchElement"
#define PCGEX_NAMESPACE PathStitch

TArray<FPCGPinProperties> UPCGExPathStitchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceEdgeSortingRules, "Sort-in-place to order the data if needed", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathStitch)

bool FPCGExPathStitchElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathStitch)

	PCGEX_FWD(DotComparisonDetails)
	Context->DotComparisonDetails.Init();

	Context->Datas.Reset(Context->MainPoints->Pairs.Num());

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExPathStitchElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathStitchElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathStitch)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs are either closed loop or have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPathStitch::FBatch>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2 || PCGExPaths::GetClosedLoop(Entry->GetIn()))
				{
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					bHasInvalidInputs = true;
					return false;
				}

				Context->Datas.Add(FPCGTaggedData(Entry->GetIn(), Entry->Tags->Flatten(), NAME_None));
				return true;
			},
			[&](const TSharedPtr<PCGExPathStitch::FBatch>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to work with."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathStitch
{
	bool FProcessor::SetStartStitch(const TSharedPtr<FProcessor>& InStitch)
	{
		if (StartStitch) { return false; }
		StartStitch = InStitch;
		return true;
	}

	bool FProcessor::SetEndStitch(const TSharedPtr<FProcessor>& InStitch)
	{
		if (EndStitch) { return false; }
		bSeedPath = WorkIndex < InStitch->WorkIndex && StartStitch == nullptr && !InStitch->IsSeed(); // Seed path if start is set while there's no end
		EndStitch = InStitch;
		return true;
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathStitch::Process);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		if (!IPointsProcessor::Process(InAsyncManager)) { return false; }
		const TConstPCGValueRange<FTransform> InTransform = PointDataFacade->GetIn()->GetConstTransformValueRange();

		StartSegment = PCGExMath::FSegment(InTransform[1].GetLocation(), InTransform[0].GetLocation(), Settings->Tolerance);
		EndSegment = PCGExMath::FSegment(InTransform[InTransform.Num() - 2].GetLocation(), InTransform[InTransform.Num() - 1].GetLocation(), Settings->Tolerance);

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (!IsSeed())
		{
			// If not stitched to anything, just forward the path as-is
			if (!EndStitch && !StartStitch) { PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Forward); }
			return;
		}

		PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New);
		Merger = MakeShared<FPCGExPointIOMerger>(PointDataFacade);

		TSharedPtr<FProcessor> Start = SharedThis(this);
		TSharedPtr<FProcessor> PreviousProcessor = Start;
		TSharedPtr<FProcessor> NextProcessor = EndStitch;
		bool bReverse = false;
		bool bClosedLoop = false;

		int32 ReadStart = 0;
		int32 ReadCount = Start->PointDataFacade->GetNum();

		Merger->Append(
			PreviousProcessor->PointDataFacade->Source,
			static_cast<PCGExMT::FScope>(NextProcessor->PointDataFacade->GetInScope(ReadStart, ReadCount)));

		while (NextProcessor)
		{
			if (NextProcessor->StartStitch != PreviousProcessor) { bReverse = !bReverse; }

			ReadStart = 0;
			ReadCount = NextProcessor->PointDataFacade->GetNum();

			if (Settings->Method == EPCGExStitchMethod::Fuse)
			{
				ReadCount--;
				if (Settings->FuseMethod == EPCGExStitchFuseMethod::KeepEnd) { ReadStart++; }
			}

			PCGExPointIOMerger::FMergeScope& MergeScope = Merger->Append(
				NextProcessor->PointDataFacade->Source,
				static_cast<PCGExMT::FScope>(NextProcessor->PointDataFacade->GetInScope(ReadStart, ReadCount)));

			MergeScope.bReverse = bReverse;

			TSharedPtr<FProcessor> OldPrev = PreviousProcessor;
			PreviousProcessor = NextProcessor;
			NextProcessor = NextProcessor->StartStitch == OldPrev ? NextProcessor->EndStitch : NextProcessor->StartStitch;

			if (NextProcessor == Start)
			{
				// That's a closed loop!
				bClosedLoop = true;
				NextProcessor = nullptr;
			}
		}

		Merger->MergeAsync(AsyncManager, &Context->CarryOverDetails);

		PCGExPaths::SetClosedLoop(PointDataFacade->GetOut(), bClosedLoop);
	}

	void FProcessor::Write()
	{
		if (!PointDataFacade->Source->IsForwarding())
		{
			// TODO : Stitch-Merge points (average etc)
			PointDataFacade->WriteFastest(AsyncManager);
		}
	}

	FBatch::FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: TBatch(InContext, InPointsCollection)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathStitch);
	}

	void FBatch::OnInitialPostProcess()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathStitch);

		UE_LOG(LogTemp, Warning, TEXT("PathStitch OnInitialPostProcess"));

		TBatch<FProcessor>::OnInitialPostProcess();

		TArray<TSharedPtr<FProcessor>> SortedProcessors;
		SortedProcessors.Reserve(Processors.Num());

		for (const TSharedRef<FProcessor>& Processor : this->Processors) { SortedProcessors.Add(Processor); }

		// Attempt to sort -- if it fails it's ok, just throw a warning

		TArray<FPCGExSortRuleConfig> RuleConfigs = PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules);
		if (!RuleConfigs.IsEmpty())
		{
			const TSharedPtr<PCGExSorting::FPointSorter> Sorter = MakeShared<PCGExSorting::FPointSorter>(RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;

			if (Sorter->Init(Context, Context->Datas))
			{
				SortedProcessors.Sort(
					[&](const TSharedPtr<FProcessor>& A, const TSharedPtr<FProcessor>& B)
					{
						return Sorter->SortData(A->BatchIndex, B->BatchIndex);
					});
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Problem with initializing sorting rules."));
			}
		}

		// Build data octree
		TSharedPtr<PCGEx::FIndexedItemOctree> PathOctree = MakeShared<PCGEx::FIndexedItemOctree>();
		for (int i = 0; i < SortedProcessors.Num(); ++i)
		{
			const TSharedPtr<FProcessor> Processor = SortedProcessors[i];
			Processor->WorkIndex = i;
			PathOctree->AddElement(PCGEx::FIndexedItem(Processor->BatchIndex, Processor->PointDataFacade->GetIn()->GetBounds()));
		}

		// ---A---x x---B---
		auto CanStitch = [&](const PCGExMath::FSegment& A, const PCGExMath::FSegment& B)
		{
			if (FVector::Dist(A.B, B.B) > Settings->Tolerance) { return false; }
			if (Settings->bDoRequireAlignment && !Context->DotComparisonDetails.Test(FVector::DotProduct(A.Direction, B.Direction * -1))) { return false; }
			return true;
		};

		// Resolve stitching
		for (int i = 0; i < SortedProcessors.Num(); ++i)
		{
			TSharedPtr<FProcessor> Current = SortedProcessors[i];
			if (!Current->IsAvailableForStitching()) { continue; }

			// Find candidates that could connect to this path' end first
			if (!Current->EndStitch)
			{
				const PCGExMath::FSegment& CurrentSegment = Current->EndSegment;
				PathOctree->FindFirstElementWithBoundsTest(
					CurrentSegment.Bounds, [&](const PCGEx::FIndexedItem& Item)
					{
						if (Current->EndStitch) { return false; }

						const TSharedPtr<FProcessor>& OtherProcessor = Processors[Item.Index];

						// Ignore anterior working paths
						if (OtherProcessor->WorkIndex == Current->WorkIndex ||
							OtherProcessor->WorkIndex < Current->WorkIndex) { return true; }

						bool bStitched = false;
						if (CanStitch(CurrentSegment, Current->StartSegment) && OtherProcessor->SetStartStitch(Current)) { bStitched = true; }
						else if (!Settings->bOnlyMatchStartAndEnds && CanStitch(CurrentSegment, Current->EndSegment) && OtherProcessor->SetEndStitch(Current)) { bStitched = true; }

						if (bStitched)
						{
							bStitched = Current->SetEndStitch(OtherProcessor);
							check(bStitched);
						}

						return !bStitched;
					});
			}

			if (!Current->StartStitch)
			{
				const PCGExMath::FSegment& CurrentSegment = Current->StartSegment;
				PathOctree->FindFirstElementWithBoundsTest(
					CurrentSegment.Bounds, [&](const PCGEx::FIndexedItem& Item)
					{
						if (Current->StartStitch) { return false; }

						const TSharedPtr<FProcessor>& OtherProcessor = Processors[Item.Index];

						// Ignore anterior working paths
						if (OtherProcessor->WorkIndex == Current->WorkIndex ||
							OtherProcessor->WorkIndex < Current->WorkIndex) { return true; }

						bool bStitched = false;
						if (CanStitch(CurrentSegment, Current->EndSegment) && OtherProcessor->SetEndStitch(Current)) { bStitched = true; }
						else if (!Settings->bOnlyMatchStartAndEnds && CanStitch(CurrentSegment, Current->EndSegment) && OtherProcessor->SetStartStitch(Current)) { bStitched = true; }

						if (bStitched)
						{
							bStitched = Current->SetStartStitch(OtherProcessor);
							check(bStitched);
						}

						return !bStitched;
					});
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExReversePointOrder.h"


#define LOCTEXT_NAMESPACE "PCGExReversePointOrderElement"
#define PCGEX_NAMESPACE ReversePointOrder

PCGExData::EIOInit UPCGExReversePointOrderSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExReversePointOrderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (bReverseUsingSortingRules)
	{
		PCGEX_PIN_PARAMS(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ReversePointOrder)

bool FPCGExReversePointOrderElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ReversePointOrder)

	for (const FPCGExSwapAttributePairDetails& OriginalPair : Settings->SwapAttributesValues)
	{
		if (!OriginalPair.Validate(Context)) { return false; }
	}

	return true;
}

bool FPCGExReversePointOrderElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExReversePointOrderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ReversePointOrder)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>>& NewBatch)
			{
				NewBatch->bPrefetchData = Settings->bReverseUsingSortingRules || !Settings->SwapAttributesValues.IsEmpty();
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExReversePointOrder
{
	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TPointsProcessor<FPCGExReversePointOrderContext, UPCGExReversePointOrderSettings>::RegisterBuffersDependencies(FacadePreloader);

		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(PointDataFacade->GetIn()->Metadata);

		for (const FPCGExSwapAttributePairDetails& OriginalPair : Settings->SwapAttributesValues)
		{
			PCGEx::FAttributeIdentity* FirstIdentity = AttributesInfos->Find(OriginalPair.FirstAttributeName);
			PCGEx::FAttributeIdentity* SecondIdentity = AttributesInfos->Find(OriginalPair.SecondAttributeName);
			if (!FirstIdentity || !SecondIdentity) { continue; }
			if (FirstIdentity->UnderlyingType != SecondIdentity->UnderlyingType) { continue; }

			const int32 AddIndex = SwapPairs.Add(OriginalPair);

			SwapPairs[AddIndex].FirstIdentity = FirstIdentity;
			SwapPairs[AddIndex].SecondIdentity = SecondIdentity;

			FacadePreloader.Register(Context, *FirstIdentity);
			FacadePreloader.Register(Context, *SecondIdentity);
		}

		if (Settings->bReverseUsingSortingRules)
		{
			Sorter = MakeShared<PCGExSorting::PointSorter<false, true>>(Context, PointDataFacade, PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
			Sorter->SortDirection = Settings->SortDirection;
			Sorter->RegisterBuffersDependencies(FacadePreloader);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		if (Sorter)
		{
			if (!Sorter->Init())
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some sorting rules could not be processed."));
				bReversed = false;
				PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
				return false;
			}

			if (!Sorter->Sort(0, PointDataFacade->GetNum() - 1))
			{
				bReversed = false;
				PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
				return true;
			}
		}

		PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::DuplicateInput);

		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
		Algo::Reverse(MutablePoints);

		if (SwapPairs.IsEmpty()) { return true; } // Swap pairs are built during data prefetch

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchWritersTask)
		FetchWritersTask->OnCompleteCallback = [&]() { StartParallelLoopForPoints(); };
		FetchWritersTask->OnSubLoopStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::FetchWriters);

				FPCGExSwapAttributePairDetails& WorkingPair = SwapPairs[StartIndex];

				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(WorkingPair.FirstIdentity->UnderlyingType), [&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						WorkingPair.FirstWriter = PointDataFacade->GetWritable<RawT>(WorkingPair.FirstAttributeName, PCGExData::EBufferInit::Inherit);
						WorkingPair.SecondWriter = PointDataFacade->GetWritable<RawT>(WorkingPair.SecondAttributeName, PCGExData::EBufferInit::Inherit);
					});
			};

		FetchWritersTask->StartSubLoops(SwapPairs.Num(), 1);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		FPointsProcessor::PrepareSingleLoopScopeForPoints(StartIndex, Count);
		for (const FPCGExSwapAttributePairDetails& WorkingPair : SwapPairs)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(WorkingPair.FirstIdentity->UnderlyingType), [&](auto DummyValue) -> void
				{
					using RawT = decltype(DummyValue);
					TSharedPtr<PCGExData::TBuffer<RawT>> FirstWriter = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(WorkingPair.FirstWriter);
					TSharedPtr<PCGExData::TBuffer<RawT>> SecondWriter = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(WorkingPair.SecondWriter);

					if (WorkingPair.bMultiplyByMinusOne)
					{
						for (int i = 0; i < Count; i++)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Read(Index);
							FirstWriter->GetMutable(Index) = PCGExMath::DblMult(SecondWriter->GetConst(Index), -1);
							SecondWriter->GetMutable(Index) = PCGExMath::DblMult(FirstValue, -1);
						}
					}
					else
					{
						for (int i = 0; i < Count; i++)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Read(Index);
							FirstWriter->GetMutable(Index) = SecondWriter->GetConst(Index);
							SecondWriter->GetMutable(Index) = FirstValue;
						}
					}
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		if (bReversed)
		{
			if (!SwapPairs.IsEmpty()) { PointDataFacade->Write(AsyncManager); }
			if (Settings->bTagIfReversed) { PointDataFacade->Source->Tags->Add(Settings->IsReversedTag); }
		}
		else
		{
			if (Settings->bTagIfNotReversed) { PointDataFacade->Source->Tags->Add(Settings->IsNotReversedTag); }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

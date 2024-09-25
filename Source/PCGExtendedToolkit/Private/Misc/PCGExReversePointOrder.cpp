// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExReversePointOrder.h"





#define LOCTEXT_NAMESPACE "PCGExReversePointOrderElement"
#define PCGEX_NAMESPACE ReversePointOrder

PCGExData::EInit UPCGExReversePointOrderSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(ReversePointOrder)

FPCGExReversePointOrderContext::~FPCGExReversePointOrderContext()
{
	PCGEX_TERMINATE_ASYNC
}

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

	PCGEX_CONTEXT(ReversePointOrder)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExReversePointOrder
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		Algo::Reverse(MutablePoints);

		AttributesInfos = PCGEx::FAttributesInfos::Get(PointDataFacade->GetIn()->Metadata);

		for (const FPCGExSwapAttributePairDetails& OriginalPair : Settings->SwapAttributesValues)
		{
			PCGEx::FAttributeIdentity* FirstIdentity = AttributesInfos->Find(OriginalPair.FirstAttributeName);
			PCGEx::FAttributeIdentity* SecondIdentity = AttributesInfos->Find(OriginalPair.SecondAttributeName);
			if (!FirstIdentity || !SecondIdentity) { continue; }
			if (FirstIdentity->UnderlyingType != SecondIdentity->UnderlyingType) { continue; }

			const int32 AddIndex = SwapPairs.Add(OriginalPair);

			SwapPairs[AddIndex].FirstIdentity = FirstIdentity;
			SwapPairs[AddIndex].SecondIdentity = SecondIdentity;
		}

		if (SwapPairs.IsEmpty()) { return true; }

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchWritersTask)
		FetchWritersTask->SetOnCompleteCallback([&]() { StartParallelLoopForPoints(); });
		FetchWritersTask->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::FetchWriters);

				FPCGExSwapAttributePairDetails& WorkingPair = SwapPairs[StartIndex];

				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(WorkingPair.FirstIdentity->UnderlyingType), [&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						WorkingPair.FirstWriter = PointDataFacade->GetWritable<RawT>(WorkingPair.FirstAttributeName, false);
						WorkingPair.SecondWriter = PointDataFacade->GetWritable<RawT>(WorkingPair.SecondAttributeName, false);
					});
			});

		FetchWritersTask->PrepareRangesOnly(SwapPairs.Num(), 1);

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
					PCGExData::TBuffer<RawT>* FirstWriter = static_cast<PCGExData::TBuffer<RawT>*>(WorkingPair.FirstWriter);
					PCGExData::TBuffer<RawT>* SecondWriter = static_cast<PCGExData::TBuffer<RawT>*>(WorkingPair.SecondWriter);

					if (WorkingPair.bMultiplyByMinusOne)
					{
						for (int i = 0; i < Count; ++i)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Read(Index);
							FirstWriter->GetMutable(Index) = PCGExMath::DblMult(SecondWriter->GetMutable(Index), -1);
							SecondWriter->GetMutable(Index) = PCGExMath::DblMult(FirstValue, -1);
						}
					}
					else
					{
						for (int i = 0; i < Count; ++i)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Read(Index);
							FirstWriter->GetMutable(Index) = SecondWriter->GetMutable(Index);
							SecondWriter->GetMutable(Index) = FirstValue;
						}
					}
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

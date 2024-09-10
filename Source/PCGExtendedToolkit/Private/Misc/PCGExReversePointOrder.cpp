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
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ReversePointOrder)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

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

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, FetchWritersTask)
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
						WorkingPair.FirstWriter = PointDataFacade->GetWriter<RawT>(WorkingPair.FirstAttributeName, false);
						WorkingPair.SecondWriter = PointDataFacade->GetWriter<RawT>(WorkingPair.SecondAttributeName, false);
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
					PCGEx::TAttributeWriter<RawT>* FirstWriter = static_cast<PCGEx::TAttributeWriter<RawT>*>(WorkingPair.FirstWriter);
					PCGEx::TAttributeWriter<RawT>* SecondWriter = static_cast<PCGEx::TAttributeWriter<RawT>*>(WorkingPair.SecondWriter);

					if (WorkingPair.bMultiplyByMinusOne)
					{
						for (int i = 0; i < Count; i++)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Values[Index];
							FirstWriter->Values[Index] = PCGExMath::DblMult(SecondWriter->Values[Index], -1);
							SecondWriter->Values[Index] = PCGExMath::DblMult(FirstValue, -1);
						}
					}
					else
					{
						for (int i = 0; i < Count; i++)
						{
							const int32 Index = StartIndex + i;
							const RawT FirstValue = FirstWriter->Values[Index];
							FirstWriter->Values[Index] = SecondWriter->Values[Index];
							SecondWriter->Values[Index] = FirstValue;
						}
					}
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

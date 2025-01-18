// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMergePoints.h"


#define LOCTEXT_NAMESPACE "PCGExMergePointsElement"
#define PCGEX_NAMESPACE MergePoints

PCGExData::EIOInit UPCGExMergePointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(MergePoints)

TArray<FPCGPinProperties> UPCGExMergePointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(GetMainOutputPin(), "The merged points.", Required, {})
	return PinProperties;
}

bool FPCGExMergePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergePoints)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	PCGEX_FWD(TagsToAttributes)
	Context->TagsToAttributes.Init();

	return true;
}

bool FPCGExMergePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsElement::Execute);

	PCGEX_CONTEXT(MergePoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExMergePoints::FBatch>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExMergePoints::FBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to merge."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->CompositeDataFacade->Source->StageOutput();

	return Context->TryComplete();
}

namespace PCGExMergePoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergePoints::FProcessor::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bTagToAttributes)
		{
			NumPoints = PointDataFacade->GetNum();
			ConvertedTagsList = ConvertedTags->Array();

			StartParallelLoopForRange(ConvertedTagsList.Num(), 1);
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		FName AttributeName = ConvertedTagsList[Iteration];
		FString Tag = AttributeName.ToString();

		if (const TSharedPtr<PCGExTags::FTagValue> TagValue = PointDataFacade->Source->Tags->GetValue(Tag))
		{
			bool bTryBroadcast = false;
			PCGEx::ExecuteWithRightType(
				TagValue->UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const T Value = StaticCastSharedPtr<PCGExTags::TTagValue<T>>(TagValue)->Value;
					TSharedPtr<PCGExData::TBuffer<T>> Buffer = Context->CompositeDataFacade->GetWritable(AttributeName, T{}, true, PCGExData::EBufferInit::New);

					// Value type mismatch
					if (!Buffer)
					{
						bTryBroadcast = true;
						return;
					}

					for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->GetMutable(i) = Value; }
				});

			if (!bTryBroadcast) { return; }

			// Handle type mismatch by broadcasting the value with whatever type was discovered first
			const TSharedPtr<PCGExData::FBufferBase> UntypedBuffer = Context->CompositeDataFacade->FindReadableAttributeBuffer(AttributeName);
			if (!UntypedBuffer) { return; }

			PCGEx::ExecuteWithRightType(
				UntypedBuffer->GetType(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					T Value = T{};

					PCGEx::ExecuteWithRightType(
						TagValue->UnderlyingType, [&](auto DummyValue2)
						{
							using RawT = decltype(DummyValue2);
							Value = PCGEx::Broadcast<T>(StaticCastSharedPtr<PCGExTags::TTagValue<RawT>>(TagValue)->Value);
						});

					TSharedPtr<PCGExData::TBuffer<T>> Buffer = Context->CompositeDataFacade->GetWritable(AttributeName, T{}, true, PCGExData::EBufferInit::New);

					if (!Buffer)
					{
						bTryBroadcast = false;
						return;
					}

					for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->GetMutable(i) = Value; }
				});

			if (!bTryBroadcast) { return; }
		}

		// Fallback to bool

		const bool HasTag = PointDataFacade->Source->Tags->IsTagged(Tag);
		TSharedPtr<PCGExData::TBuffer<bool>> Buffer = Context->CompositeDataFacade->GetWritable(AttributeName, false, true, PCGExData::EBufferInit::New);

		if (!Buffer) { return; }

		for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->GetMutable(i) = HasTag; }
	}

	FBatch::FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: TBatch(InContext, InPointsCollection)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergePoints);

		TSharedPtr<PCGExData::FPointIO> CompositeIO = PCGExData::NewPointIO(InContext, Settings->GetMainOutputPin(), 0);
		CompositeIO->InitializeOutput(PCGExData::EIOInit::New);

		ConvertedTags = MakeShared<TSet<FName>>();

		PCGEX_MAKE_SHARED(CompositeDataFacade, PCGExData::FFacade, CompositeIO.ToSharedRef());
		Context->CompositeDataFacade = CompositeDataFacade;
		Merger = MakeShared<FPCGExPointIOMerger>(CompositeDataFacade.ToSharedRef());
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& PointsProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(PointsProcessor)) { return false; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergePoints);

		PointsProcessor->OutScope = Merger->Append(PointsProcessor->PointDataFacade->Source);
		PointsProcessor->ConvertedTags = ConvertedTags;

		if (Settings->bTagToAttributes)
		{
			ConvertedTags->Append(PointsProcessor->PointDataFacade->Source->Tags->ToFNameList(false));
		}

		return true;
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		StartMerge();
		/*
		PCGEX_LAUNCH(
			PCGExMT::FDeferredCallbackTask,
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartMerge();
			});
			*/
	}

	void FBatch::Write()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergePoints::FBatch::Write);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergePoints);
		Context->CompositeDataFacade->Write(AsyncManager);
	}

	void FBatch::StartMerge()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsElement::StartMerge);
		// TODO : Implement a method in IOMerger to easily feedb attributes we'll need to o facade preloader

		FPCGExMergePointsContext* Context = GetContext<FPCGExMergePointsContext>();
		Context->TagsToAttributes.Prune(*ConvertedTags.Get()); // Keep only desired conversions

		// Make sure we ignore attributes that should not be brought out
		IgnoredAttributes.Append(*ConvertedTags.Get());
		IgnoredAttributes.Append(
			{
				PCGExGraph::Tag_EdgeEndpoints,
				PCGExGraph::Tag_VtxEndpoint,
				PCGExGraph::Tag_ClusterIndex,
				PCGExGraph::Tag_ClusterPair,
				PCGExGraph::Tag_PCGExVtx,
				PCGExGraph::Tag_PCGExEdges,
			});

		// Launch all merging tasks while we compute future attributes 
		Merger->MergeAsync(AsyncManager, &Context->CarryOverDetails, &IgnoredAttributes);

		// Cleanup tags that are used internally for data recognition, along with the tags we will be converting to data
		Context->CompositeDataFacade->Source->Tags->Remove(IgnoredAttributes);

		TBatch<FProcessor>::OnProcessingPreparationComplete(); //!
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

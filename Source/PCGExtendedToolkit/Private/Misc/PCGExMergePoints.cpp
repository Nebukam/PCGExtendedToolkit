﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMergePoints.h"


#include "Data/PCGExDataTag.h"
#include "Graph/PCGExEdge.h"


#define LOCTEXT_NAMESPACE "PCGExMergePointsElement"
#define PCGEX_NAMESPACE MergePoints

PCGEX_INITIALIZE_ELEMENT(MergePoints)
PCGEX_ELEMENT_BATCH_POINT_IMPL_ADV(MergePoints)

TArray<FPCGPinProperties> UPCGExMergePointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(GetMainOutputPin(), "The merged points.", Required)
	return PinProperties;
}

bool FPCGExMergePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergePoints)

	PCGEX_FWD(SortingDetails)
	if (!Context->SortingDetails.Init(Context)) { return false; }

	Context->SortingDetails.Sort(Context, Context->MainPoints);

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
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to merge."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	(void)Context->CompositeDataFacade->Source->StageOutput(Context);

	return Context->TryComplete();
}

namespace PCGExMergePoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergePoints::FProcessor::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bTagToAttributes)
		{
			NumPoints = PointDataFacade->GetNum();
			ConvertedTagsList = ConvertedTags->Array();

			StartParallelLoopForRange(ConvertedTagsList.Num(), 1);
		}

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			FName AttributeName = ConvertedTagsList[Index];
			FString Tag = AttributeName.ToString();

			if (const TSharedPtr<PCGExData::IDataValue> TagValue = PointDataFacade->Source->Tags->GetValue(Tag))
			{
				bool bTryBroadcast = false;
				PCGEx::ExecuteWithRightType(
					TagValue->UnderlyingType, [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						const T Value = StaticCastSharedPtr<PCGExData::TDataValue<T>>(TagValue)->Value;
						TSharedPtr<PCGExData::TBuffer<T>> Buffer = Context->CompositeDataFacade->GetWritable(AttributeName, T{}, true, PCGExData::EBufferInit::New);

						// Value type mismatch
						if (!Buffer)
						{
							bTryBroadcast = true;
							return;
						}

						for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->SetValue(i, Value); }
					});

				if (!bTryBroadcast) { continue; }

				// Handle type mismatch by broadcasting the value with whatever type was discovered first
				const TSharedPtr<PCGExData::IBuffer> UntypedBuffer = Context->CompositeDataFacade->FindReadableAttributeBuffer(AttributeName);
				if (!UntypedBuffer) { continue; }

				PCGEx::ExecuteWithRightType(
					UntypedBuffer->GetType(), [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						T Value = T{};

						PCGEx::ExecuteWithRightType(
							TagValue->UnderlyingType, [&](auto DummyValue2)
							{
								using T_REAL = decltype(DummyValue2);
								Value = PCGEx::FSubSelection().Get<T_REAL, T>(StaticCastSharedPtr<PCGExData::TDataValue<T_REAL>>(TagValue)->Value);
							});

						TSharedPtr<PCGExData::TBuffer<T>> Buffer = Context->CompositeDataFacade->GetWritable(AttributeName, T{}, true, PCGExData::EBufferInit::New);

						if (!Buffer)
						{
							bTryBroadcast = false;
							return;
						}

						for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->SetValue(i, Value); }
					});

				if (!bTryBroadcast)
				{
				}

				continue; // This is a value tag, not a simple tag, stop processing here.
			}

			if (PointDataFacade->Source->Tags->IsTagged(Tag))
			{
				FWriteScopeLock WriteScopeLock(SimpleTagsLock);
				SimpleTags.Add(FName(AttributeName));
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		TProcessor<FPCGExMergePointsContext, UPCGExMergePointsSettings>::OnRangeProcessingComplete();

		check(Context);

		if (SimpleTags.IsEmpty()) { return; }

		for (TArray<FName> SimpleTagNames = SimpleTags.Array();
		     const FName TagName : SimpleTagNames)
		{
			// First validate that we're not overriding a tag with the same name that's not a bool
			if (const FPCGMetadataAttributeBase* FlagAttribute = Context->CompositeDataFacade->Source->GetOut()->Metadata->GetConstAttribute(TagName);
				FlagAttribute && FlagAttribute->GetTypeId() != static_cast<int16>(EPCGMetadataTypes::Boolean))
			{
				if (!Settings->bQuietTagOverlapWarning)
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Overlap between regular tag & value tag '{0}', and the value is not a bool."), FText::FromName(TagName)));
				}
				continue;
			}

			TSharedPtr<PCGExData::TBuffer<bool>> Buffer = Context->CompositeDataFacade->GetWritable(TagName, false, true, PCGExData::EBufferInit::New);

			if (!Buffer) { continue; }

			for (int i = OutScope.Start; i < OutScope.End; i++) { Buffer->SetValue(i, true); }
		}
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

	bool FBatch::PrepareSingle(const TSharedRef<PCGExPointsMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergePoints);
		PCGEX_TYPED_PROCESSOR_REF

		TypedProcessor->OutScope = Merger->Append(InProcessor->PointDataFacade->Source).Write;
		TypedProcessor->ConvertedTags = ConvertedTags;

		if (Settings->bTagToAttributes)
		{
			ConvertedTags->Append(InProcessor->PointDataFacade->Source->Tags->FlattenToArrayOfNames(false));
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
		Context->CompositeDataFacade->WriteFastest(AsyncManager);
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
				PCGExGraph::Attr_PCGExEdgeIdx,
				PCGExGraph::Attr_PCGExVtxIdx,
				PCGExGraph::Tag_PCGExCluster,
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

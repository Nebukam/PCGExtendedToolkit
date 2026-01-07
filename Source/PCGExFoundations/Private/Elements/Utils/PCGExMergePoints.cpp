// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Utils/PCGExMergePoints.h"

#include "Clusters/PCGExClusterCommon.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Types/PCGExTypes.h"
#include "Utils/PCGExPointIOMerger.h"

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

bool FPCGExMergePointsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
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
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to merge."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	(void)Context->CompositeDataFacade->Source->StageOutput(Context);

	return Context->TryComplete();
}

namespace PCGExMergePoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergePoints::FProcessor::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

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
				PCGExMetaHelpers::ExecuteWithRightType(TagValue->GetTypeId(), [&](auto DummyValue)
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

				const TSharedPtr<PCGExData::IBuffer> WritableBuffer = Context->CompositeDataFacade->GetWritable(UntypedBuffer->GetTypeId(), AttributeName, PCGExData::EBufferInit::New);
				if (!WritableBuffer) { continue; }

				const PCGExTypeOps::ITypeOpsBase* BufferOps = PCGExTypeOps::FTypeOpsRegistry::Get(UntypedBuffer->GetTypeId());
				const PCGExTypeOps::ITypeOpsBase* TagOps = PCGExTypeOps::FTypeOpsRegistry::Get(TagValue->GetTypeId());

				{
					PCGExTypes::FScopedTypedValue Value(BufferOps->GetTypeId());
					if (!BufferOps->SameType(TagOps))
					{
						PCGExTypes::FScopedTypedValue SourceValue(TagOps->GetTypeId());
						TagValue->GetVoid(SourceValue.GetRaw());
						BufferOps->ConvertFrom(TagOps->GetTypeId(), SourceValue.GetRaw(), Value.GetRaw());
					}
					else
					{
						TagValue->GetVoid(Value.GetRaw());
					}

					for (int i = OutScope.Start; i < OutScope.End; i++) { WritableBuffer->SetVoid(i, Value.GetRaw()); }
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

		for (TArray<FName> SimpleTagNames = SimpleTags.Array(); const FName TagName : SimpleTagNames)
		{
			// First validate that we're not overriding a tag with the same name that's not a bool
			if (const FPCGMetadataAttributeBase* FlagAttribute = Context->CompositeDataFacade->Source->GetOut()->Metadata->GetConstAttribute(TagName); FlagAttribute && FlagAttribute->GetTypeId() != static_cast<int16>(EPCGMetadataTypes::Boolean))
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
	}

	void FBatch::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergePoints::FBatch::Write);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergePoints);
		Context->CompositeDataFacade->WriteFastest(TaskManager);
	}

	void FBatch::StartMerge()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergePointsElement::StartMerge);
		// TODO : Implement a method in IOMerger to easily feedb attributes we'll need to o facade preloader

		FPCGExMergePointsContext* Context = GetContext<FPCGExMergePointsContext>();
		Context->TagsToAttributes.Prune(*ConvertedTags.Get()); // Keep only desired conversions

		// Make sure we ignore attributes that should not be brought out
		IgnoredAttributes.Append(*ConvertedTags.Get());
		IgnoredAttributes.Append({PCGExClusters::Labels::Attr_PCGExEdgeIdx, PCGExClusters::Labels::Attr_PCGExVtxIdx, PCGExClusters::Labels::Tag_PCGExCluster, PCGExClusters::Labels::Tag_PCGExVtx, PCGExClusters::Labels::Tag_PCGExEdges,});

		{
			PCGEX_SCHEDULING_SCOPE(TaskManager);

			// Launch all merging tasks while we compute future attributes 
			Merger->MergeAsync(TaskManager, &Context->CarryOverDetails, &IgnoredAttributes);

			// Cleanup tags that are used internally for data recognition, along with the tags we will be converting to data
			Context->CompositeDataFacade->Source->Tags->Remove(IgnoredAttributes);

			TBatch<FProcessor>::OnProcessingPreparationComplete(); //!
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExAttributeRemap.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRemap"
#define PCGEX_NAMESPACE AttributeRemap


PCGExData::EInit UPCGExAttributeRemapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(AttributeRemap)

bool FPCGExAttributeRemapElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	PCGEX_VALIDATE_NAME(Settings->SourceAttributeName)
	PCGEX_VALIDATE_NAME(Settings->TargetAttributeName)

	Context->RemapSettings[0] = Settings->BaseRemap;
	Context->RemapSettings[1] = Settings->Component2RemapOverride;
	Context->RemapSettings[2] = Settings->Component3RemapOverride;
	Context->RemapSettings[3] = Settings->Component4RemapOverride;

	for (int i = 0; i < 4; i++) { Context->RemapSettings[i].RemapDetails.LoadCurve(); }

	Context->RemapIndices[0] = 0;
	Context->RemapIndices[1] = Settings->bOverrideComponent2 ? 1 : 0;
	Context->RemapIndices[2] = Settings->bOverrideComponent3 ? 2 : 0;
	Context->RemapIndices[3] = Settings->bOverrideComponent4 ? 3 : 0;

	return true;
}

bool FPCGExAttributeRemapElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemapElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAttributeRemap::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAttributeRemap::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to remap."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAttributeRemap
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(PointDataFacade->GetIn()->Metadata);
		const PCGEx::FAttributeIdentity* Identity = Infos->Find(Settings->SourceAttributeName);

		if (!Identity)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Some inputs are missing the specified source attribute."));
			return false;
		}

		UnderlyingType = Identity->UnderlyingType;

		switch (UnderlyingType)
		{
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
			Dimensions = 1;
			break;
		case EPCGMetadataTypes::Vector2:
			Dimensions = 2;
			break;
		case EPCGMetadataTypes::Vector:
		case EPCGMetadataTypes::Rotator:
			Dimensions = 3;
			break;
		case EPCGMetadataTypes::Vector4:
		case EPCGMetadataTypes::Quaternion:
			Dimensions = 4;
			break;
		default:
		case EPCGMetadataTypes::Transform:
		case EPCGMetadataTypes::String:
		case EPCGMetadataTypes::Boolean:
		case EPCGMetadataTypes::Name:
		case EPCGMetadataTypes::Unknown:
			Dimensions = -1;
			break;
		}

		if (Dimensions == -1)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Source attribute type cannot be remapped."));
			return false;
		}

		if (!Identity)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Some inputs are missing the specified source attribute."));
			return false;
		}


		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(UnderlyingType), [&](auto DummyValue) -> void
			{
				using RawT = decltype(DummyValue);
				CacheWriter = PointDataFacade->GetWritable<RawT>(Settings->TargetAttributeName, true);
				CacheReader = PointDataFacade->GetScopedReadable<RawT>(Identity->Name);
			});

		Rules.Reserve(Dimensions);
		for (int i = 0; i < Dimensions; i++)
		{
			FPCGExComponentRemapRule Rule = Rules.Add_GetRef(FPCGExComponentRemapRule(Context->RemapSettings[Context->RemapIndices[i]]));
			Rule.RemapDetails.InMin = MAX_dbl;
			Rule.RemapDetails.InMax = MIN_dbl;
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchTask)
		FetchTask->OnCompleteCallback =
			[&]()
			{
				// Fix min/max range
				for (FPCGExComponentRemapRule& Rule : Rules)
				{
					if (Rule.RemapDetails.bUseInMin) { Rule.RemapDetails.InMin = Rule.RemapDetails.CachedInMin; }
					else { for (const double Min : Rule.MinCache) { Rule.RemapDetails.InMin = FMath::Min(Rule.RemapDetails.InMin, Min); } }

					if (Rule.RemapDetails.bUseInMax) { Rule.RemapDetails.InMax = Rule.RemapDetails.CachedInMax; }
					else { for (const double Max : Rule.MaxCache) { Rule.RemapDetails.InMax = FMath::Max(Rule.RemapDetails.InMax, Max); } }

					if (Rule.RemapDetails.RangeMethod == EPCGExRangeType::FullRange) { Rule.RemapDetails.InMin = 0; }
				}

				OnPreparationComplete();
			};

		FetchTask->OnIterationRangePrepareCallback =
			[&](const TArray<uint64>& Loops)
			{
				for (FPCGExComponentRemapRule& Rule : Rules)
				{
					Rule.MinCache.Init(MAX_dbl, Loops.Num());
					Rule.MaxCache.Init(MIN_dbl, Loops.Num());
				}
			};

		FetchTask->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::Fetch);

				PointDataFacade->Fetch(StartIndex, Count);
				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(UnderlyingType), [&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						TSharedPtr<PCGExData::TBuffer<RawT>> Writer = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(CacheWriter);
						TSharedPtr<PCGExData::TBuffer<RawT>> Reader = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(CacheReader);

						// TODO : Swap for a scoped accessor since we don't need to keep readable values in memory
						for (int i = StartIndex; i < StartIndex + Count; i++) { Writer->GetMutable(i) = Reader->Read(i); } // Copy range to writer

						// Find min/max & clamp values

						for (int d = 0; d < Dimensions; d++)
						{
							FPCGExComponentRemapRule& Rule = Rules[d];

							double Min = MAX_dbl;
							double Max = MIN_dbl;

							if (Rule.RemapDetails.bUseAbsoluteRange)
							{
								for (int i = StartIndex; i < StartIndex + Count; i++)
								{
									RawT& V = Writer->GetMutable(i);
									const double VAL = Rule.InputClampDetails.GetClampedValue(PCGExMath::GetComponent(V, d));
									PCGExMath::SetComponent(V, d, VAL);

									Min = FMath::Min(Min, FMath::Abs(VAL));
									Max = FMath::Max(Max, FMath::Abs(VAL));
								}
							}
							else
							{
								for (int i = StartIndex; i < StartIndex + Count; i++)
								{
									RawT& V = Writer->GetMutable(i);
									const double VAL = Rule.InputClampDetails.GetClampedValue(PCGExMath::GetComponent(V, d));
									PCGExMath::SetComponent(V, d, VAL);

									Min = FMath::Min(Min, VAL);
									Max = FMath::Max(Max, VAL);
								}
							}

							Rule.MinCache[LoopIdx] = Min;
							Rule.MaxCache[LoopIdx] = Max;
						}
					});
			};

		FetchTask->PrepareRangesOnly(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::OnPreparationComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, RemapTask)
		RemapTask->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(UnderlyingType), [&](auto DummyValue) -> void
					{
						RemapRange(StartIndex, Count, DummyValue);
					});
			};

		RemapTask->PrepareRangesOnly(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExAttributeRemap.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRemap"
#define PCGEX_NAMESPACE AttributeRemap


PCGExData::EIOInit UPCGExAttributeRemapSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

#if WITH_EDITOR
FString UPCGExAttributeRemapSettings::GetDisplayName() const
{
	if (Attributes.bOutputToDifferentName)
	{
		return Attributes.Source.ToString() + " → " + Attributes.Target.ToString();
	}
	return Attributes.Source.ToString();
}
#endif

void FPCGExAttributeRemapContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	for (const FPCGExComponentRemapRule& Rule : RemapSettings) { AddAssetDependency(Rule.RemapDetails.RemapCurve.ToSoftObjectPath()); }
}

PCGEX_INITIALIZE_ELEMENT(AttributeRemap)

void UPCGExAttributeRemapSettings::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	if (SourceAttributeName_DEPRECATED != NAME_None)
	{
		Attributes.Source = SourceAttributeName_DEPRECATED;
		SourceAttributeName_DEPRECATED = NAME_None;
	}

	if (TargetAttributeName_DEPRECATED != NAME_None)
	{
		Attributes.Target = TargetAttributeName_DEPRECATED;
		TargetAttributeName_DEPRECATED = NAME_None;

		Attributes.bOutputToDifferentName = (SourceAttributeName_DEPRECATED != TargetAttributeName_DEPRECATED);
	}
#endif
}

bool FPCGExAttributeRemapElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->Attributes.Source)
	if (Settings->Attributes.bOutputToDifferentName) { PCGEX_VALIDATE_NAME(Settings->Attributes.Target) }

	Context->RemapSettings[0] = Settings->BaseRemap;
	Context->RemapSettings[1] = Settings->Component2RemapOverride;
	Context->RemapSettings[2] = Settings->Component3RemapOverride;
	Context->RemapSettings[3] = Settings->Component4RemapOverride;

	return true;
}

void FPCGExAttributeRemapElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	for (int i = 0; i < 4; i++) { Context->RemapSettings[i].RemapDetails.Init(); }

	Context->RemapIndices[0] = 0;
	Context->RemapIndices[1] = Settings->bOverrideComponent2 ? 1 : 0;
	Context->RemapIndices[2] = Settings->bOverrideComponent3 ? 2 : 0;
	Context->RemapIndices[3] = Settings->bOverrideComponent4 ? 3 : 0;
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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(PointDataFacade->GetIn()->Metadata);
		const PCGEx::FAttributeIdentity* Identity = Infos->Find(Settings->Attributes.Source);

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

		PCGEx::ExecuteWithRightType(
			UnderlyingType, [&](auto DummyValue)
			{
				using RawT = decltype(DummyValue);
				CacheWriter = PointDataFacade->GetWritable<RawT>(Settings->Attributes.GetOutputName(), PCGExData::EBufferInit::New);
				CacheReader = PointDataFacade->GetScopedReadable<RawT>(Identity->Name);
			});

		Rules.Reserve(Dimensions);
		for (int i = 0; i < Dimensions; i++)
		{
			FPCGExComponentRemapRule& Rule = Rules.Add_GetRef(FPCGExComponentRemapRule(Context->RemapSettings[Context->RemapIndices[i]]));
			Rule.RemapDetails.InMin = MAX_dbl;
			Rule.RemapDetails.InMax = MIN_dbl_neg;
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchTask)

		FetchTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS

				// Fix min/max range
				for (FPCGExComponentRemapRule& Rule : This->Rules)
				{
					if (!Rule.RemapDetails.bUseInMin)
					{
						Rule.RemapDetails.InMin = Rule.MinCache->Flatten([&](const double& In, const double& Out) { return FMath::Min(In, Out); });
					}

					if (!Rule.RemapDetails.bUseInMax)
					{
						Rule.RemapDetails.InMax = Rule.MaxCache->Flatten([&](const double& In, const double& Out) { return FMath::Max(In, Out); });
					}

					if (Rule.RemapDetails.RangeMethod == EPCGExRangeType::FullRange) { Rule.RemapDetails.InMin = 0; }
				}

				This->OnPreparationComplete();
			};

		FetchTask->OnPrepareSubLoopsCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops)
			{
				PCGEX_ASYNC_THIS

				for (FPCGExComponentRemapRule& Rule : This->Rules)
				{
					Rule.MinCache = MakeShared<PCGExMT::TScopedValue<double>>(Loops, MAX_dbl);
					Rule.MaxCache = MakeShared<PCGExMT::TScopedValue<double>>(Loops, MIN_dbl_neg);
				}
			};

		FetchTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::Fetch);

				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);
				PCGEx::ExecuteWithRightType(
					This->UnderlyingType, [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						TSharedPtr<PCGExData::TBuffer<RawT>> Writer = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(This->CacheWriter);
						TSharedPtr<PCGExData::TBuffer<RawT>> Reader = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(This->CacheReader);

						// TODO : Swap for a scoped accessor since we don't need to keep readable values in memory
						for (int i = Scope.Start; i < Scope.End; i++) { Writer->GetMutable(i) = Reader->Read(i); } // Copy range to writer

						// Find min/max & clamp values

						for (int d = 0; d < This->Dimensions; d++)
						{
							FPCGExComponentRemapRule& Rule = This->Rules[d];

							double Min = MAX_dbl;
							double Max = MIN_dbl_neg;

							if (Rule.RemapDetails.bUseAbsoluteRange)
							{
								for (int i = Scope.Start; i < Scope.End; i++)
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
								for (int i = Scope.Start; i < Scope.End; i++)
								{
									RawT& V = Writer->GetMutable(i);
									const double VAL = Rule.InputClampDetails.GetClampedValue(PCGExMath::GetComponent(V, d));
									PCGExMath::SetComponent(V, d, VAL);

									Min = FMath::Min(Min, VAL);
									Max = FMath::Max(Max, VAL);
								}
							}

							Rule.MinCache->Set(Scope, Min);
							Rule.MaxCache->Set(Scope, Max);
						}
					});
			};

		FetchTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::OnPreparationComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, RemapTask)
		RemapTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				PCGEx::ExecuteWithRightType(
					This->UnderlyingType, [&](auto DummyValue)
					{
						This->RemapRange(Scope, DummyValue);
					});
			};

		RemapTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

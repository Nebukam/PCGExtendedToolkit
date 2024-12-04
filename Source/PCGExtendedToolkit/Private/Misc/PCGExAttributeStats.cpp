// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExAttributeStats.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

#define PCGEX_FOREACH_STAT(MACRO, _TYPE)\
MACRO(Identifier, FString, TEXT("None"))\
MACRO(DefaultValue, _TYPE, _TYPE{})\
MACRO(MinValue, _TYPE, _TYPE{})\
MACRO(MaxValue, _TYPE, _TYPE{})\
MACRO(SetMinValue, _TYPE, _TYPE{})\
MACRO(SetMaxValue, _TYPE, _TYPE{})\
MACRO(AverageValue, _TYPE, _TYPE{})\
MACRO(UniqueValuesNum, int32, 0)\
MACRO(UniqueSetValuesNum, int32, 0)\
MACRO(DefaultValuesNum, int32, 0)\
MACRO(HasOnlyDefaultValues, bool, false)\
MACRO(HasOnlySetValues, bool, false)\
MACRO(HasOnlyUniqueValues, bool, false)\
MACRO(Samples, int32, 0)\
MACRO(IsValid, bool, false)


#define LOCTEXT_NAMESPACE "PCGExAttributeStats"
#define PCGEX_NAMESPACE AttributeStats

TArray<FPCGPinProperties> UPCGExAttributeStatsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_PARAMS(PCGExAttributeStats::OutputAttributeStats, "Per-attribute stats, one row per input dataset.", Required, {})
	if (bOutputPerUniqueValuesStats)
	{
		PCGEX_PIN_PARAMS(PCGExAttributeStats::OutputAttributeUniqueValues, "Per-dataset, per-attribute unique values.", Normal, {})
	}
	return PinProperties;
}

PCGExData::EIOInit UPCGExAttributeStatsSettings::GetMainOutputInitMode() const { return OutputToPoints == EPCGExStatsOutputToPoints::None ? PCGExData::EIOInit::Forward : PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(AttributeStats)

bool FPCGExAttributeStatsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }
	PCGEX_CONTEXT_AND_SETTINGS(AttributeStats)

	FPCGExNameFiltersDetails Filters = Settings->Filters;
	Filters.Init();

	Context->AttributesInfos = MakeShared<PCGEx::FAttributesInfos>();
	TSet<FName> OutMismatch;

	TSet<FName> UniqueNames;
#define PCGEX_STAT_CHECK(_NAME, _TYPE, _DEFAULT) if(Settings->bOutput##_NAME){	PCGEX_VALIDATE_NAME(Settings->_NAME##AttributeName);\
	bool bAlreadySet = false; UniqueNames.Add(Settings->_NAME##AttributeName, &bAlreadySet);\
	if(bAlreadySet){ PCGE_LOG(Error, GraphAndLog, FText::Format(FTEXT("Duplicate attribute name: {0}."), FText::FromName(Settings->_NAME##AttributeName))); }}
	PCGEX_FOREACH_STAT(PCGEX_STAT_CHECK, nullptr)
#undef PCGEX_STAT_CHECK

	for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
	{
		TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(IO->GetIn()->Metadata);
		Context->AttributesInfos->Append(Infos, OutMismatch);
	}

	if (!OutMismatch.IsEmpty() && !Settings->bQuietTypeMismatchWarning)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Some attributes share the same name but not the same type; only the first type found will be processed."));
	}

	if (Context->AttributesInfos->Identities.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No attributes found!"));
		return false;
	}

	Filters.Prune(*Context->AttributesInfos, true);

	if (Settings->bFeedbackLoopFailsafe)
	{
		TArray<FString> Affixes;
		Affixes.Reserve(15);
#define PCGEX_STAT_FILTER(_NAME, _TYPE, _DEFAULT) if(Settings->bOutput##_NAME){ Affixes.Add(Settings->_NAME##AttributeName.ToString()); }
		PCGEX_FOREACH_STAT(PCGEX_STAT_FILTER, bool)
#undef PCGEX_STAT_FILTER

		Context->AttributesInfos->Filter(
			[&Affixes](const FName& InName)
			{
				const FString StrName = InName.ToString();
				for (const FString& Affix : Affixes) { if (StrName.StartsWith(Affix) || StrName.EndsWith(Affix)) { return false; } }
				return true;
			});
	}

	if (Context->AttributesInfos->Identities.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The node does not output any data after filtering is applied."));
		return false;
	}

	const int32 NumRows = Context->MainPoints->Num();

#define PCGEX_STAT_DECL(_NAME, _TYPE, _DEFAULT) if(Settings->bOutput##_NAME){ NewParamData->Metadata->FindOrCreateAttribute<_TYPE>(Settings->_NAME##AttributeName, _DEFAULT); }

	Context->Rows.Reserve(NumRows);
	Context->OutputParams.Reserve(Context->AttributesInfos->Identities.Num());
	for (const PCGEx::FAttributeIdentity& Identity : Context->AttributesInfos->Identities)
	{
		UPCGParamData* NewParamData = Context->ManagedObjects->New<UPCGParamData>();
		Context->OutputParams.Add(NewParamData);
		Context->OutputParamsMap.Add(Identity.Name, NewParamData);

		for (int i = 0; i < NumRows; i++) { Context->Rows.Add(NewParamData->Metadata->AddEntry()); }

			PCGEx::ExecuteWithRightType(
                                                            					Identity.UnderlyingType, [&](auto DummyValue)
                                                            					{
				using T = decltype(DummyValue);
				PCGEX_FOREACH_STAT(PCGEX_STAT_DECL, T)
			});
	}
#undef PCGEX_STAT_DECL

	return true;
}

bool FPCGExAttributeStatsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeStatsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeStats)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAttributeStats::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAttributeStats::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	for (int i = 0; i < Context->OutputParams.Num(); i++)
	{
		UPCGParamData* ParamData = Context->OutputParams[i];
		Context->StageOutput(
			PCGExAttributeStats::OutputAttributeStats, ParamData,
			{Context->AttributesInfos->Attributes[i]->Name.ToString()},
			false, false);
	}

	return Context->TryComplete();
}

namespace PCGExAttributeStats
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeStats::Process);

		// Must be set before process for filters
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const int64 Key = Context->Rows[PointDataFacade->Source->IOIndex];
		const int32 NumAttributes = Context->AttributesInfos->Identities.Num();

		if (Settings->bOutputPerUniqueValuesStats)
		{
			PerAttributeStatMap.Reserve(NumAttributes);
			PerAttributeStats.Init(nullptr, NumAttributes);
		}

		Stats.Reserve(NumAttributes);
		for (int i = 0; i < NumAttributes; i++)
		{
			const PCGEx::FAttributeIdentity& Identity = Context->AttributesInfos->Identities[i];

			if (Settings->bOutputPerUniqueValuesStats) { PerAttributeStatMap.Add(Identity.Name, i); }

			PCGEx::ExecuteWithRightType(
                                                                        					Identity.UnderlyingType, [&](auto DummyValue)
                                                                        					{
					using RawT = decltype(DummyValue);
					TSharedPtr<TAttributeStats<RawT>> S = MakeShared<TAttributeStats<RawT>>(Identity, Key);
					Stats.Add(StaticCastSharedPtr<FAttributeStatsBase>(S));
				});
		}


		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FilterScope)

		FilterScope->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->Fetch(StartIndex, Count);
				This->FilterScope(StartIndex, Count);
			};

		FilterScope->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, AttributeStatProcessing)
		AttributeStatProcessing->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGEX_ASYNC_THIS
				This->Stats[StartIndex]->Process(This->PointDataFacade, This->Context, This->Settings, This->PointFilterCache);
			};

		AttributeStatProcessing->StartSubLoops(Stats.Num(), 1);
	}
}

#undef PCGEX_FOREACH_STAT

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

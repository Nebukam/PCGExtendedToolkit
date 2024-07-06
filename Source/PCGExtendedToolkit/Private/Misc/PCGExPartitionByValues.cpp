// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPartitionByValues.h"

#include "Data/PCGExData.h"
#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionByValues"
#define PCGEX_NAMESPACE PartitionByValues

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

namespace PCGExPartition
{
	FKPartition::FKPartition(FKPartition* InParent, const int64 InKey, FPCGExFilter::FRule* InRule, const int32 InPartitionIndex)
		: Parent(InParent), PartitionIndex(InPartitionIndex), PartitionKey(InKey), Rule(InRule)
	{
	}

	FKPartition::~FKPartition()
	{
		PCGEX_DELETE_TMAP(SubLayers, int64)
		UniquePartitionKeys.Empty();
	}

	int32 FKPartition::GetSubPartitionsNum()
	{
		if (SubLayers.IsEmpty()) { return 1; }

		int32 Num = 0;
		for (const TPair<int64, FKPartition*>& Pair : SubLayers) { Num += Pair.Value->GetSubPartitionsNum(); }
		return Num;
	}

	FKPartition* FKPartition::GetPartition(const int64 Key, FPCGExFilter::FRule* InRule)
	{
		FKPartition** LayerPtr;

		{
			FReadScopeLock ReadLock(LayersLock);
			LayerPtr = SubLayers.Find(Key);
		}

		if (!LayerPtr)
		{
			FWriteScopeLock WriteLock(LayersLock);
			FKPartition* Partition = new FKPartition(this, Key, InRule, SubLayers.Num());

			UniquePartitionKeys.Add(Key);
			SubLayers.Add(Key, Partition);
			return Partition;
		}

		return *LayerPtr;
	}

	void FKPartition::Register(TArray<FKPartition*>& Partitions)
	{
		if (!SubLayers.IsEmpty())
		{
			for (const TPair<int64, FKPartition*>& Pair : SubLayers) { Pair.Value->Register(Partitions); }
		}
		else
		{
			Partitions.Add(this);
		}
	}

	void FKPartition::SortPartitions()
	{
		TMap<int64, int64> ValuesIndices;
		TArray<int64> UValues = UniquePartitionKeys.Array();
		UValues.Sort();

		int64 PIndex = 0;
		for (int64 UniquePartitionKey : UValues) { ValuesIndices.Add(UniquePartitionKey, PIndex++); }
		UValues.Empty();

		for (const TPair<int64, FKPartition*>& Pair : SubLayers)
		{
			Pair.Value->SortPartitions();
			Pair.Value->PartitionIndex = *ValuesIndices.Find(Pair.Value->PartitionKey); //Ordered index
		}

		ValuesIndices.Empty();
		UniquePartitionKeys.Empty();
	}
}

#if WITH_EDITOR
void UPCGExPartitionByValuesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExPartitonRuleConfig& Config : PartitionRules) { Config.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool UPCGExPartitionByValuesBaseSettings::GetMainAcceptMultipleData() const { return false; }


PCGExData::EInit UPCGExPartitionByValuesBaseSettings::GetMainOutputInitMode() const { return bSplitOutput ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

bool UPCGExPartitionByValuesBaseSettings::GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const
{
	return true;
}

bool UPCGExPartitionByValuesSettings::GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const
{
	if (PartitionRules.IsEmpty()) { return false; }
	for (const FPCGExPartitonRuleConfig& Config : PartitionRules) { OutRules.Add(Config); }
	return true;
}

FPCGExPartitionByValuesBaseContext::~FPCGExPartitionByValuesBaseContext()
{
	PCGEX_DELETE(RootPartition)
	KeySums.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PartitionByValuesBase)

bool FPCGExPartitionByValuesBaseElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

	TArray<FPCGExPartitonRuleConfig> Configs;
	if (!Settings->GetPartitionRules(InContext, Configs))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No partitioning rules."));
		return false;
	}

	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_C(KeySum, int64)

	for (const FPCGExPartitonRuleConfig& Config : Configs)
	{
		if (!Config.bEnabled) { continue; }

		FPCGExPartitonRuleConfig& ConfigCopy = Context->RulesConfigs.Add_GetRef(Config);

		if (Config.bWriteKey && !FPCGMetadataAttributeBase::IsValidName(Config.KeyAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Key Partition name {0} is invalid."), FText::FromName(Config.KeyAttributeName)));
			ConfigCopy.bWriteKey = false;
		}

		if (Config.bWriteTag && !FPCGMetadataAttributeBase::IsValidName(Config.TagPrefixName))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Tag Partition name {0} is invalid."), FText::FromName(Config.TagPrefixName)));
			ConfigCopy.bWriteTag = false;
		}
	}

	PCGEX_FWD(bSplitOutput)

	Context->RootPartition = new PCGExPartition::FKPartition(nullptr, 0, nullptr, -1);

	if (Context->RulesConfigs.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No partitioning rules."));
		return false;
	}

	return true;
}

bool FPCGExPartitionByValuesBaseElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionByValuesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&]()
		{
			Context->Rules.Empty(); //
			Context->CurrentIO->CreateInKeys();

			const int32 NumPoints = Context->CurrentIO->GetNum();

			if (Settings->bWriteKeySum && !Context->bSplitOutput) { Context->KeySums.SetNumZeroed(NumPoints); }

			for (FPCGExPartitonRuleConfig& Config : Context->RulesConfigs)
			{
				FPCGExFilter::FRule& NewRule = Context->Rules.Emplace_GetRef(Config);
				if (!NewRule.Grab(Context->CurrentIO)) { Context->Rules.Pop(); }
			}

			// Prepare each rule so it cache the filter key by index
			for (FPCGExFilter::FRule& Rule : Context->Rules) { Rule.FilteredValues.SetNumZeroed(NumPoints); }
		};

		auto ProcessPoint = [&](const int32 PointIndex)
		{
			PCGExPartition::FKPartition* Partition = Context->RootPartition;
			for (FPCGExFilter::FRule& Rule : Context->Rules)
			{
				const int64 KeyValue = Rule.Filter(PointIndex);
				Partition = Partition->GetPartition(KeyValue, &Rule);
				Rule.FilteredValues[PointIndex] = KeyValue;
			}

			Partition->Add(PointIndex);
		};

		if (!Context->Process(Initialize, ProcessPoint, Context->CurrentIO->GetNum())) { return false; }

		Context->RootPartition->SortPartitions();

		if (Context->bSplitOutput)
		{
			Context->NumPartitions = Context->RootPartition->GetSubPartitionsNum();
			Context->Partitions.Reserve(Context->NumPartitions);
			Context->RootPartition->Register(Context->Partitions);

			Context->SetState(PCGExPartition::State_DistributeToPartition);
		}
		else
		{
			TMap<int64, int64> IndiceMap;
			for (FPCGExFilter::FRule& Rule : Context->Rules)
			{
				if (!Rule.RuleConfig->bWriteKey) { continue; }
				if (Rule.RuleConfig->bUsePartitionIndexAsKey)
				{
					IndiceMap.Empty(Rule.FilteredValues.Num());
					int64 PIndex = -1;
					for (int64& Value : Rule.FilteredValues)
					{
						const int64* FilterPtr = IndiceMap.Find(Value);
						if (!FilterPtr)
						{
							IndiceMap.Add(Value, ++PIndex);
							Value = PIndex;
						}
						else
						{
							Value = *FilterPtr;
						}
					}
					IndiceMap.Empty();
				}

				PCGEx::FAttributeAccessor<int64>* Accessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(Context->CurrentIO, Rule.RuleConfig->KeyAttributeName, 0, false);
				Accessor->SetRange(Rule.FilteredValues);
				delete Accessor;

				if (Settings->bWriteKeySum)
				{
					for (int i = 0; i < Rule.FilteredValues.Num(); i++) { Context->KeySums[i] += Rule.FilteredValues[i]; }
					PCGEx::FAttributeAccessor<int64>* KSAccessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(Context->CurrentIO, Settings->KeySumAttributeName, 0, false);
					KSAccessor->SetRange(Context->KeySums);
					delete KSAccessor;
				}
			}

			Context->OutputMainPoints();
			Context->Done();
		}
	}

	if (Context->IsState(PCGExPartition::State_DistributeToPartition))
	{
		auto CreatePartition = [&](const int32 Index)
		{
			PCGExData::FTags* Tags = new PCGExData::FTags();
			Tags->Append(Context->CurrentIO->Tags);
			PCGExPartition::FKPartition* Partition = Context->Partitions[Index];
			const UPCGPointData* InData = Context->CurrentIO->GetIn();
			UPCGPointData* OutData = NewObject<UPCGPointData>();
			OutData->InitializeFromData(InData);

			const TArray<FPCGPoint>& InPoints = InData->GetPoints();
			TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();
			OutPoints.Reserve(Partition->Points.Num());

			for (const int32 PointIndex : Partition->Points) { OutPoints.Add(InPoints[PointIndex]); }

			int64 Sum = 0;
			while (Partition->Parent)
			{
				const FPCGExFilter::FRule* Rule = Partition->Rule;
				Sum += Partition->PartitionKey;

				if (Rule->RuleConfig->bWriteKey)
				{
					PCGExData::WriteMark<int64>(
						OutData->Metadata,
						Rule->RuleConfig->KeyAttributeName,
						Rule->RuleConfig->bUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey);
				}

				if (Rule->RuleConfig->bWriteTag)
				{
					FString TagValue;
					Tags->Set(
						Rule->RuleConfig->TagPrefixName.ToString(),
						Rule->RuleConfig->bTagUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey,
						TagValue);
				}

				Partition = Partition->Parent;
			}

			if (Settings->bWriteKeySum) { PCGExData::WriteMark<int64>(OutData->Metadata, Settings->KeySumAttributeName, Sum); }

			if (Settings->bFlattenOutput) { OutData->Metadata->Flatten(); }

			Context->FutureOutput(Context->MainPoints->DefaultOutputLabel, OutData, Tags->ToSet());
			PCGEX_DELETE(Tags)
		};

		if (!Context->Process(CreatePartition, Context->NumPartitions)) { return false; }

		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

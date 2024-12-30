// Copyright 2024 Timothé Lapetite and contributors
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
	FKPartition::FKPartition(const TWeakPtr<FKPartition>& InParent, const int64 InKey, FRule* InRule, const int32 InPartitionIndex)
		: Parent(InParent), PartitionIndex(InPartitionIndex), PartitionKey(InKey), Rule(InRule)
	{
	}

	FKPartition::~FKPartition()
	{
	}

	int32 FKPartition::GetSubPartitionsNum()
	{
		if (SubLayers.IsEmpty()) { return 1; }

		int32 Num = 0;
		for (const TPair<int64, TSharedPtr<FKPartition>>& Pair : SubLayers) { Num += Pair.Value->GetSubPartitionsNum(); }
		return Num;
	}

	TSharedPtr<FKPartition> FKPartition::GetPartition(const int64 Key, FRule* InRule)
	{
		TSharedPtr<FKPartition>* LayerPtr;

		{
			FReadScopeLock ReadLock(LayersLock);
			LayerPtr = SubLayers.Find(Key);
			if (LayerPtr) { return *LayerPtr; }
		}

		{
			FWriteScopeLock WriteLock(LayersLock);
			LayerPtr = SubLayers.Find(Key);

			if (LayerPtr) { return *LayerPtr; }

			PCGEX_SHARED_THIS_DECL
			TSharedPtr<FKPartition> Partition = MakeShared<FKPartition>(ThisPtr, Key, InRule, SubLayers.Num());

			UniquePartitionKeys.Add(Key);
			SubLayers.Add(Key, Partition);
			return Partition;
		}
	}

	void FKPartition::Register(TArray<TSharedPtr<FKPartition>>& Partitions)
	{
		if (!SubLayers.IsEmpty())
		{
			for (const TPair<int64, TSharedPtr<FKPartition>>& Pair : SubLayers) { Pair.Value->Register(Partitions); }
		}
		else
		{
			PCGEX_SHARED_THIS_DECL
			Partitions.Add(ThisPtr);
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

		for (const TPair<int64, TSharedPtr<FKPartition>>& Pair : SubLayers)
		{
			Pair.Value->SortPartitions();
			Pair.Value->PartitionIndex = *ValuesIndices.Find(Pair.Value->PartitionKey); //Ordered index
		}

		Points.Sort([](const int32& A, const int32& B) { return A < B; });

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


PCGExData::EIOInit UPCGExPartitionByValuesBaseSettings::GetMainOutputInitMode() const { return bSplitOutput ? PCGExData::EIOInit::None : PCGExData::EIOInit::Duplicate; }

bool UPCGExPartitionByValuesBaseSettings::GetPartitionRules(FPCGExContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const
{
	return true;
}

bool UPCGExPartitionByValuesSettings::GetPartitionRules(FPCGExContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const
{
	if (PartitionRules.IsEmpty()) { return false; }
	for (const FPCGExPartitonRuleConfig& Config : PartitionRules) { OutRules.Add(Config); }
	return true;
}

PCGEX_INITIALIZE_ELEMENT(PartitionByValuesBase)

bool FPCGExPartitionByValuesBaseElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

	TArray<FPCGExPartitonRuleConfig> Configs;
	if (!Settings->GetPartitionRules(InContext, Configs))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No partitioning rules."));
		return false;
	}

	if (Settings->bWriteKeySum) { PCGEX_VALIDATE_NAME(Settings->KeySumAttributeName) }

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
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPartitionByValues::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPartitionByValues::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any partitions."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPartitionByValues
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		RootPartition = MakeShared<PCGExPartition::FKPartition>(nullptr, 0, nullptr, -1);

		Rules.Empty();
		const int32 NumPoints = PointDataFacade->GetNum();

		if (Settings->bWriteKeySum && !Settings->bSplitOutput) { PCGEx::InitArray(KeySums, NumPoints); }

		FName Consumable = NAME_None;

		for (FPCGExPartitonRuleConfig& Config : Context->RulesConfigs)
		{
			const TSharedPtr<PCGExData::TBuffer<double>> DataCache = PointDataFacade->GetScopedBroadcaster<double>(Config.Selector);
			if (!DataCache) { continue; }

			if (PCGExHelpers::TryGetAttributeName(Config.Selector, PointDataFacade->Source->GetIn(), Consumable)) { Context->AddConsumableAttributeName(Consumable); }

			PCGExPartition::FRule& NewRule = Rules.Emplace_GetRef(Config);
			NewRule.DataCache = DataCache;
		}

		// Prepare each rule so it cache the filter key by index
		for (PCGExPartition::FRule& Rule : Rules) { Rule.FilteredValues.SetNumZeroed(NumPoints); }

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		TSharedPtr<PCGExPartition::FKPartition> Partition = RootPartition;
		for (PCGExPartition::FRule& Rule : Rules)
		{
			const int64 KeyValue = Rule.Filter(Index);
			Partition = Partition->GetPartition(KeyValue, &Rule);
			Rule.FilteredValues[Index] = KeyValue;
		}

		Partition->Add(Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		TSharedPtr<PCGExPartition::FKPartition> Partition = Partitions[Iteration];

		//Manually create & insert partition at the sorted IO Index
		const TSharedRef<PCGExData::FPointIO> PartitionIO = Context->MainPoints->Pairs[Partition->IOIndex].ToSharedRef();

		UPCGMetadata* Metadata = PartitionIO->GetOut()->Metadata;

		const TArray<FPCGPoint>& InPoints = PartitionIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PartitionIO->GetOut()->GetMutablePoints();
		PCGEx::InitArray(OutPoints, Partition->Points.Num());

		for (int i = 0; i < OutPoints.Num(); i++)
		{
			OutPoints[i] = InPoints[Partition->Points[i]];
			Metadata->InitializeOnSet(OutPoints[i].MetadataEntry);
		}

		int64 Sum = 0;
		while (Partition->Parent.Pin())
		{
			const PCGExPartition::FRule* Rule = Partition->Rule;
			Sum += Partition->PartitionKey;

			if (Rule->RuleConfig->bWriteKey)
			{
				PCGExData::WriteMark<int64>(
					PartitionIO,
					Rule->RuleConfig->KeyAttributeName,
					Rule->RuleConfig->bUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey);
			}

			if (Rule->RuleConfig->bWriteTag)
			{
				FString TagValue;
				PartitionIO->Tags->Add(
					Rule->RuleConfig->TagPrefixName.ToString(),
					Rule->RuleConfig->bTagUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey,
					TagValue);
			}

			Partition = Partition->Parent.Pin();
		}

		if (Settings->bWriteKeySum) { PCGExData::WriteMark<int64>(PartitionIO, Settings->KeySumAttributeName, Sum); }
	}

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
		RootPartition->SortPartitions();

		if (Settings->bSplitOutput)
		{
			NumPartitions = RootPartition->GetSubPartitionsNum();
			Partitions.Reserve(NumPartitions);
			RootPartition->Register(Partitions);

			// Sort by point index & ensure consistent output partition order

			Partitions.Sort([](const TSharedPtr<PCGExPartition::FKPartition>& A, const TSharedPtr<PCGExPartition::FKPartition>& B) { return A->Points[0] < B->Points[0]; });
			const int32 InsertOffset = Context->MainPoints->Pairs.Num();

			int32 SumPts = 0;
			for (int i = 0; i < Partitions.Num(); i++)
			{
				Partitions[i]->IOIndex = InsertOffset + i;
				Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
				SumPts += Partitions[i]->Points.Num();
			}

			StartParallelLoopForRange(NumPartitions, 64); // Too low maybe?
			return;
		}

		TMap<int64, int64> IndiceMap;
		for (PCGExPartition::FRule& Rule : Rules)
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

			const TSharedPtr<PCGExData::TBuffer<int32>> KeyWriter = PointDataFacade->GetWritable(Rule.RuleConfig->KeyAttributeName, 0, true, PCGExData::EBufferInit::New);
			for (int i = 0; i < Rule.FilteredValues.Num(); i++)
			{
				KeyWriter->GetMutable(i) = Rule.FilteredValues[i];
				if (Settings->bWriteKeySum) { KeySums[i] += Rule.FilteredValues[i]; }
			}
		}

		if (Settings->bWriteKeySum)
		{
			const TSharedPtr<PCGExData::TBuffer<int32>> KeySumWriter = PointDataFacade->GetWritable(Settings->KeySumAttributeName, 0, true, PCGExData::EBufferInit::New);
			for (int i = 0; i < KeySums.Num(); i++) { KeySumWriter->GetMutable(i) = KeySums[i]; }
		}

		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

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
			if (LayerPtr) { return *LayerPtr; }
		}

		{
			FWriteScopeLock WriteLock(LayersLock);
			LayerPtr = SubLayers.Find(Key);

			if (LayerPtr) { return *LayerPtr; }

			FKPartition* Partition = new FKPartition(this, Key, InRule, SubLayers.Num());

			UniquePartitionKeys.Add(Key);
			SubLayers.Add(Key, Partition);
			return Partition;
		}
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
	PCGEX_TERMINATE_ASYNC
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPartitionByValues::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExPartitionByValues::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any partitions."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPartitionByValues
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(RootPartition)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;

		RootPartition = new PCGExPartition::FKPartition(nullptr, 0, nullptr, -1);

		Rules.Empty(); //
		PointIO->CreateInKeys();

		const int32 NumPoints = PointIO->GetNum();

		if (Settings->bWriteKeySum && !Settings->bSplitOutput) { PCGEX_SET_NUM_UNINITIALIZED(KeySums, NumPoints) }

		for (FPCGExPartitonRuleConfig& Config : TypedContext->RulesConfigs)
		{
			FPCGExFilter::FRule& NewRule = Rules.Emplace_GetRef(Config);
			PCGExData::TCache<double>* DataCache = PointDataFacade->GetScopedBroadcaster<double>(Config.Selector);

			if (!DataCache) { Rules.Pop(); }

			NewRule.DataCache = DataCache;
		}

		// Prepare each rule so it cache the filter key by index
		for (FPCGExFilter::FRule& Rule : Rules) { Rule.FilteredValues.SetNumZeroed(NumPoints); }

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		PCGExPartition::FKPartition* Partition = RootPartition;
		for (FPCGExFilter::FRule& Rule : Rules)
		{
			const int64 KeyValue = Rule.Filter(Index);
			Partition = Partition->GetPartition(KeyValue, &Rule);
			Rule.FilteredValues[Index] = KeyValue;
		}

		Partition->Add(Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

		PCGExPartition::FKPartition* Partition = Partitions[Iteration];

		//Manually create & insert partition at the sorted IO Index
		PCGExData::FPointIO* PartitionIO = TypedContext->MainPoints->Pairs[Partition->IOIndex];

		UPCGMetadata* Metadata = PartitionIO->GetOut()->Metadata;

		const TArray<FPCGPoint>& InPoints = PartitionIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PartitionIO->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(OutPoints, Partition->Points.Num())

		for (int i = 0; i < OutPoints.Num(); i++)
		{
			OutPoints[i] = InPoints[Partition->Points[i]];
			Metadata->InitializeOnSet(OutPoints[i].MetadataEntry);
		}

		int64 Sum = 0;
		while (Partition->Parent)
		{
			const FPCGExFilter::FRule* Rule = Partition->Rule;
			Sum += Partition->PartitionKey;

			if (Rule->RuleConfig->bWriteKey)
			{
				PCGExData::WriteMark<int64>(
					Metadata,
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

			Partition = Partition->Parent;
		}

		if (Settings->bWriteKeySum) { PCGExData::WriteMark<int64>(Metadata, Settings->KeySumAttributeName, Sum); }
	}

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
		RootPartition->SortPartitions();

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionByValuesBase)

		if (Settings->bSplitOutput)
		{
			NumPartitions = RootPartition->GetSubPartitionsNum();
			Partitions.Reserve(NumPartitions);
			RootPartition->Register(Partitions);

			// Sort by point index & ensure consistent output partition order

			Partitions.Sort([](const PCGExPartition::FKPartition& A, PCGExPartition::FKPartition& B) { return A.Points[0] < B.Points[0]; });
			const int32 InsertOffset = TypedContext->MainPoints->Pairs.Num();

			int32 SumPts = 0;
			for (int i = 0; i < Partitions.Num(); i++)
			{
				Partitions[i]->IOIndex = InsertOffset + i;
				TypedContext->MainPoints->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
				SumPts += Partitions[i]->Points.Num();
			}

			StartParallelLoopForRange(NumPartitions, 64); // Too low maybe?
			return;
		}

		TMap<int64, int64> IndiceMap;
		for (FPCGExFilter::FRule& Rule : Rules)
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

			PCGEx::TAttributeWriter<int32>* KeyWriter = PointDataFacade->GetWriter(Rule.RuleConfig->KeyAttributeName, 0, false, true);
			for (int i = 0; i < Rule.FilteredValues.Num(); i++)
			{
				KeyWriter->Values[i] = Rule.FilteredValues[i];
				if (Settings->bWriteKeySum) { KeySums[i] += Rule.FilteredValues[i]; }
			}
		}

		if (Settings->bWriteKeySum)
		{
			PCGEx::TAttributeWriter<int32>* KeySumWriter = PointDataFacade->GetWriter(Settings->KeySumAttributeName, 0, false, true);
			for (int i = 0; i < KeySums.Num(); i++) { KeySumWriter->Values[i] = KeySums[i]; }
		}

		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

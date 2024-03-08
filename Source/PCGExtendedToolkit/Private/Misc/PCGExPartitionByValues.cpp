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

	void FKPartition::Add(const int64 Index)
	{
		FWriteScopeLock WriteLock(PointLock);
		Points.Add(Index);
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
	for (FPCGExFilterRuleDescriptor& Descriptor : PartitionRules) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool UPCGExPartitionByValuesSettings::GetMainAcceptMultipleData() const { return false; }


PCGExData::EInit UPCGExPartitionByValuesSettings::GetMainOutputInitMode() const { return bSplitOutput ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

FPCGExPartitionByValuesContext::~FPCGExPartitionByValuesContext()
{
	PCGEX_DELETE(RootPartition)
	KeySums.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PartitionByValues)

bool FPCGExPartitionByValuesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValues)

	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER(KeySum)

	for (const FPCGExFilterRuleDescriptor& Descriptor : Settings->PartitionRules)
	{
		if (!Descriptor.bEnabled) { continue; }

		FPCGExFilterRuleDescriptor& DescriptorCopy = Context->RulesDescriptors.Add_GetRef(Descriptor);

		if (Descriptor.bWriteKey && !FPCGMetadataAttributeBase::IsValidName(Descriptor.KeyAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Key Partition name {0} is invalid."), FText::FromName(Descriptor.KeyAttributeName)));
			DescriptorCopy.bWriteKey = false;
		}

		if (Descriptor.bWriteTag && !FPCGMetadataAttributeBase::IsValidName(Descriptor.TagPrefixName))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Tag Partition name {0} is invalid."), FText::FromName(Descriptor.TagPrefixName)));
			DescriptorCopy.bWriteTag = false;
		}
	}

	PCGEX_FWD(bSplitOutput)

	Context->RootPartition = new PCGExPartition::FKPartition(nullptr, 0, nullptr, -1);

	if (Context->RulesDescriptors.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No partitioning rules."));
		return false;
	}

	return true;
}

bool FPCGExPartitionByValuesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionByValuesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValues)

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
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->Rules.Empty(); //
			PointIO.CreateInKeys();

			const int32 NumPoints = PointIO.GetNum();

			if (Settings->bWriteKeySum && !Context->bSplitOutput) { Context->KeySums.SetNumZeroed(NumPoints); }

			for (FPCGExFilterRuleDescriptor& Descriptor : Context->RulesDescriptors)
			{
				FPCGExFilter::FRule& NewRule = Context->Rules.Emplace_GetRef(Descriptor);
				if (!NewRule.Grab(PointIO)) { Context->Rules.Pop(); }
			}

			// Prepare each rule so it cache the filter key by index
			for (FPCGExFilter::FRule& Rule : Context->Rules) { Rule.FilteredValues.SetNumZeroed(NumPoints); }
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
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

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }

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
				if (!Rule.RuleDescriptor->bWriteKey) { continue; }
				if (Rule.RuleDescriptor->bUsePartitionIndexAsKey)
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

				PCGEx::FAttributeAccessor<int64>* Accessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(*Context->CurrentIO, Rule.RuleDescriptor->KeyAttributeName, 0, false);
				Accessor->SetRange(Rule.FilteredValues);
				delete Accessor;

				if (Settings->bWriteKeySum)
				{
					for (int i = 0; i < Rule.FilteredValues.Num(); i++) { Context->KeySums[i] += Rule.FilteredValues[i]; }
					PCGEx::FAttributeAccessor<int64>* KSAccessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(*Context->CurrentIO, Settings->KeySumAttributeName, 0, false);
					KSAccessor->SetRange(Context->KeySums);
					delete KSAccessor;
				}
			}

			Context->OutputPoints();
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
			const UPCGPointData* InData = Context->GetCurrentIn();
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

				if (Rule->RuleDescriptor->bWriteKey)
				{
					PCGExData::WriteMark<int64>(
						OutData->Metadata,
						Rule->RuleDescriptor->KeyAttributeName,
						Rule->RuleDescriptor->bUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey);
				}

				if (Rule->RuleDescriptor->bWriteTag)
				{
					FString TagValue;
					Tags->Set(
						Rule->RuleDescriptor->TagPrefixName.ToString(),
						Rule->RuleDescriptor->bTagUsePartitionIndexAsKey ? Partition->PartitionIndex : Partition->PartitionKey,
						TagValue);
				}

				Partition = Partition->Parent;
			}

			if (Settings->bWriteKeySum) { PCGExData::WriteMark<int64>(OutData->Metadata, Settings->KeySumAttributeName, Sum); }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			OutData->Metadata->Flatten();	
#endif
			FPCGTaggedData* TaggedData = Context->Output(OutData, Context->MainPoints->DefaultOutputLabel);
			Tags->Dump(TaggedData->Tags);
			PCGEX_DELETE(Tags)
		};

		if (!Context->Process(CreatePartition, Context->NumPartitions, true)) { return false; }
		Context->Done();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

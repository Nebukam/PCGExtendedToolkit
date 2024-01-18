// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPartitionByValues.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionByValues"
#define PCGEX_NAMESPACE PartitionByValues

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

namespace PCGExPartition
{
	FKPartition::FKPartition(FKPartition* InParent, const int64 InKey, FPCGExFilter::FRule* InRule)
		: Parent(InParent), PartitionKey(InKey), Rule(InRule)
	{
	}

	FKPartition::~FKPartition()
	{
		TArray<int64> Keys;
		SubLayers.GetKeys(Keys);
		for (const int64 Key : Keys) { delete *SubLayers.Find(Key); }
		SubLayers.Empty();
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
			FKPartition* Partition = new FKPartition(this, Key, InRule);

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
}

PCGEX_INITIALIZE_ELEMENT(PartitionByValues)

bool FPCGExPartitionByValuesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionByValues)

	for (const FPCGExFilterRuleDescriptor& Descriptor : Settings->PartitionRules)
	{
		if (!Descriptor.bEnabled) { continue; }

		FPCGExFilterRuleDescriptor& DescriptorCopy = Context->RulesDescriptors.Add_GetRef(Descriptor);

		if (Descriptor.bWriteKey && !FPCGMetadataAttributeBase::IsValidName(Descriptor.KeyAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Key Partition name {0} is invalid."));
			DescriptorCopy.bWriteKey = false;
		}
	}

	PCGEX_FWD(bSplitOutput)

	Context->RootPartition = new PCGExPartition::FKPartition(nullptr, 0, nullptr);

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

	PCGEX_CONTEXT(PartitionByValues)

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
			Context->Rules.Empty();
			PointIO.CreateInKeys();

			for (FPCGExFilterRuleDescriptor& Descriptor : Context->RulesDescriptors)
			{
				FPCGExFilter::FRule& NewRule = Context->Rules.Emplace_GetRef(Descriptor);
				if (!NewRule.Grab(PointIO)) { Context->Rules.Pop(); }
			}

			// Prepare each rule so it cache the filter key by index
			if (!Context->bSplitOutput)
			{
				const int32 NumPoints = PointIO.GetNum();
				for (FPCGExFilter::FRule& Rule : Context->Rules)
				{
					if (!Rule.RuleDescriptor->bWriteKey) { continue; }
					Rule.FilteredValues.SetNumZeroed(NumPoints);
				}
			}
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			PCGExPartition::FKPartition* Partition = Context->RootPartition;
			for (FPCGExFilter::FRule& Rule : Context->Rules)
			{
				const int64 KeyValue = Rule.Filter(PointIndex);
				Partition = Partition->GetPartition(KeyValue, &Rule);
				if (!Context->bSplitOutput && Rule.RuleDescriptor->bWriteKey) { Rule.FilteredValues[PointIndex] = KeyValue; }
			}

			Partition->Add(PointIndex);
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }

		if (Context->bSplitOutput)
		{
			Context->NumPartitions = Context->RootPartition->GetSubPartitionsNum();
			Context->Partitions.Reserve(Context->NumPartitions);
			Context->RootPartition->Register(Context->Partitions);

			Context->SetState(PCGExPartition::State_DistributeToPartition);
		}
		else
		{
			for (FPCGExFilter::FRule& Rule : Context->Rules)
			{
				if (!Rule.RuleDescriptor->bWriteKey) { continue; }
				PCGEx::FAttributeAccessor<int64>* Accessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(*Context->CurrentIO, Rule.RuleDescriptor->KeyAttributeName, 0, false);
				Accessor->SetRange(Rule.FilteredValues);
				delete Accessor;
			}

			Context->OutputPoints();
			Context->Done();
		}
	}

	if (Context->IsState(PCGExPartition::State_DistributeToPartition))
	{
		auto CreatePartition = [&](const int32 Index)
		{
			PCGExPartition::FKPartition* Partition = Context->Partitions[Index];
			const UPCGPointData* InData = Context->GetCurrentIn();
			UPCGPointData* OutData = NewObject<UPCGPointData>();
			OutData->InitializeFromData(InData);

			const TArray<FPCGPoint>& InPoints = InData->GetPoints();
			TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();
			OutPoints.Reserve(Partition->Points.Num());

			for (const int32 PointIndex : Partition->Points) { OutPoints.Add(InPoints[PointIndex]); }

			while (Partition->Parent)
			{
				const FPCGExFilter::FRule* Rule = Partition->Rule;

				if (Rule->RuleDescriptor->bWriteKey)
				{
					PCGExData::WriteMark<int64>(
						OutData->Metadata,
						Rule->RuleDescriptor->KeyAttributeName,
						Partition->PartitionKey);
				}

				Partition = Partition->Parent;
			}

			Context->Output(OutData, Context->MainPoints->DefaultOutputLabel);
		};

		if (!Context->Process(CreatePartition, Context->NumPartitions)) { return false; }
		Context->Done();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPartitionByValues.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionByValues"

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR

FString FPCGExPartitionRuleDescriptor::GetDisplayName() const
{
	if (bEnabled) { return FPCGExInputDescriptorWithSingleField::GetDisplayName(); }
	return "(Disabled) " + FPCGExInputDescriptorWithSingleField::GetDisplayName();
}

namespace PCGExPartition
{
	FKPartition::FKPartition(FKPartition* InParent, int64 InKey, FRule* InRule)
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

	FKPartition* FKPartition::GetPartition(const int64 Key, FRule* InRule)
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

	FRule::~FRule()
	{
		Values.Empty();
		RuleDescriptor = nullptr;
	}

	int64 FRule::GetPartitionKey(const FPCGPoint& Point) const
	{
		const double Upscaled = GetValue(Point) * Upscale + Offset;
		const double Filtered = (Upscaled - FMath::Fmod(Upscaled, FilterSize)) / FilterSize;
		return static_cast<int64>(Filtered);
	}
}


void UPCGExPartitionByValuesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExPartitionRuleDescriptor& Descriptor : PartitionRules) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UPCGExPartitionByValuesSettings::GetMainPointsInputAcceptMultipleData() const { return false; }

#endif

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const { return MakeShared<FPCGExPartitionByValuesElement>(); }
PCGExData::EInit UPCGExPartitionByValuesSettings::GetPointOutputInitMode() const { return bSplitOutput ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

FPCGExSplitByValuesContext::~FPCGExSplitByValuesContext()
{
	PCGEX_DELETE(RootPartition)
}

FPCGContext* FPCGExPartitionByValuesElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExSplitByValuesContext* Context = new FPCGExSplitByValuesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExPartitionByValuesSettings* Settings = Context->GetInputSettings<UPCGExPartitionByValuesSettings>();
	check(Settings);

	for (const FPCGExPartitionRuleDescriptor& Descriptor : Settings->PartitionRules)
	{
		if (!Descriptor.bEnabled) { continue; }

		FPCGExPartitionRuleDescriptor& DescriptorCopy = Context->RulesDescriptors.Add_GetRef(Descriptor);

		if (Descriptor.bWriteKey && !FPCGMetadataAttributeBase::IsValidName(Descriptor.KeyAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedAttributeName", "Key Partition name {0} is invalid."));
			DescriptorCopy.bWriteKey = false;
		}
	}

	Context->bSplitOutput = Settings->bSplitOutput;
	Context->RootPartition = new PCGExPartition::FKPartition(nullptr, 0, nullptr);
	return Context;
}

bool FPCGExPartitionByValuesElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExSplitByValuesContext* Context = static_cast<FPCGExSplitByValuesContext*>(InContext);

	if (Context->RulesDescriptors.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingRules", "No partitioning rules."));
		return false;
	}

	return true;
}

bool FPCGExPartitionByValuesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBucketEntryElement::Execute);

	FPCGExSplitByValuesContext* Context = static_cast<FPCGExSplitByValuesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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

			for (FPCGExPartitionRuleDescriptor& Descriptor : Context->RulesDescriptors)
			{
				PCGExPartition::FRule& NewRule = Context->Rules.Emplace_GetRef(Descriptor);
				if (!NewRule.Validate(PointIO.GetIn())) { Context->Rules.Pop(); }
			}

			// Prepare each rule so it cache the filter key by index
			if (!Context->bSplitOutput)
			{
				const int32 NumPoints = PointIO.GetNum();
				for (PCGExPartition::FRule& Rule : Context->Rules)
				{
					if (!Rule.RuleDescriptor->bWriteKey) { continue; }
					Rule.Values.SetNumZeroed(NumPoints);
				}
			}
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);
			PCGExPartition::FKPartition* Partition = Context->RootPartition;
			for (PCGExPartition::FRule& Rule : Context->Rules)
			{
				const int64 KeyValue = Rule.GetPartitionKey(Point);
				Partition = Partition->GetPartition(KeyValue, &Rule);
				if (!Context->bSplitOutput && Rule.RuleDescriptor->bWriteKey) { Rule.Values[PointIndex] = KeyValue; }
			}

			Partition->Add(PointIndex);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			if (Context->bSplitOutput)
			{
				Context->NumPartitions = Context->RootPartition->GetSubPartitionsNum();
				Context->Partitions.Reserve(Context->NumPartitions);
				Context->RootPartition->Register(Context->Partitions);

				Context->SetState(PCGExPartition::State_DistributeToPartition);
			}
			else
			{
				for (PCGExPartition::FRule& Rule : Context->Rules)
				{
					if (!Rule.RuleDescriptor->bWriteKey) { continue; }
					PCGEx::FAttributeAccessor<int64>* Accessor = PCGEx::FAttributeAccessor<int64>::FindOrCreate(*Context->CurrentIO, Rule.RuleDescriptor->KeyAttributeName, 0, false);
					Accessor->SetRange(Rule.Values);
					delete Accessor;
				}

				Context->OutputPoints();
				Context->Done();
			}
		}
	}

	if (Context->IsState(PCGExPartition::State_DistributeToPartition))
	{
		auto CreatePartitions = [&](const int32 Index)
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
				const PCGExPartition::FRule* Rule = Partition->Rule;

				if (Rule->RuleDescriptor->bWriteKey)
				{
					PCGEx::CreateMark(
						OutData->Metadata,
						Rule->RuleDescriptor->KeyAttributeName,
						Partition->PartitionKey);
				}

				Partition = Partition->Parent;
			}

			Context->Output(OutData, Context->MainPoints->DefaultOutputLabel);
		};

		if (Context->Process(CreatePartitions, Context->NumPartitions)) { Context->Done(); }
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE

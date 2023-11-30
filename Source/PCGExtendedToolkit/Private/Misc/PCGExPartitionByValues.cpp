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
	else { return "(Disabled) " + FPCGExInputDescriptorWithSingleField::GetDisplayName(); }
}

void UPCGExPartitionByValuesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExPartitionRuleDescriptor& Descriptor : PartitionRules) { Descriptor.PrintDisplayName(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const { return MakeShared<FPCGExPartitionByValuesElement>(); }

PCGExIO::EInitMode UPCGExPartitionByValuesSettings::GetPointOutputInitMode() const
{
	return bSplitOutput ? PCGExIO::EInitMode::NoOutput : PCGExIO::EInitMode::DuplicateInput;
}

void FPCGExSplitByValuesContext::PrepareForPoints(const UPCGExPointIO* PointIO)
{
	FWriteScopeLock WriteLock(RulesLock);

	Rules.Empty();

	for (FPCGExPartitionRuleDescriptor& Descriptor : RulesDescriptors)
	{
		PCGExPartition::FRule& NewRule = Rules.Emplace_GetRef(Descriptor);
		if (!NewRule.Validate(PointIO->In))
		{
			//PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedRule", "Rule %s invalid on input %s."));
			Rules.Pop();
			continue;
		}

		if (Descriptor.bWriteKey)
		{
			NewRule.KeyAttribute = PointIO->Out->Metadata->FindOrCreateAttribute<int64>(Descriptor.KeyAttributeName, 0, false, true);
		}
		else
		{
			NewRule.KeyAttribute = nullptr;
		}
	}
}

void FPCGExSplitByValuesContext::PrepareForPointsWithMetadataEntries(UPCGExPointIO* PointIO)
{
	PointIO->BuildMetadataEntries();
	PrepareForPoints(PointIO);
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
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedAttributeName", "Key Partition name %s is invalid."));
			DescriptorCopy.bWriteKey = false;
		}
	}

	Context->bSplitOutput = Settings->bSplitOutput;
	Context->RootPartitionLayer = NewObject<UPCGExPartitionLayer>();
	return Context;
}

bool FPCGExPartitionByValuesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBucketEntryElement::Execute);

	FPCGExSplitByValuesContext* Context = static_cast<FPCGExSplitByValuesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	////

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto Initialize = [&](UPCGExPointIO* PointIO)
	{
		if (Context->bSplitOutput) { Context->PrepareForPoints(PointIO); }
		else { Context->PrepareForPointsWithMetadataEntries(PointIO); }
	};

	// Only write partition values
	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetOutPoint(PointIndex);
		TArray<int64> LayerKeys;
		UPCGExPartitionLayer* Layer = Context->RootPartitionLayer;

		for (PCGExPartition::FRule& Rule : Context->Rules)
		{
			if (Rule.KeyAttribute)
			{
				Rule.KeyAttribute->SetValue(Point.MetadataEntry, Rule.Filter(Point));
			}
		}
	};

	// Split output
	auto DistributePoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetInPoint(PointIndex);

		TArray<int64> LayerKeys;
		UPCGExPartitionLayer* Layer = Context->RootPartitionLayer;

		for (PCGExPartition::FRule& Rule : Context->Rules)
		{
			const int64 LayerKey = Rule.Filter(Point);
			LayerKeys.Add(LayerKey);
			Layer = Layer->GetLayer(LayerKey, PointIO->In);
		}

		if (!Layer->Points)
		{
			FWriteScopeLock WriteLock(Context->ContextLock);

			Layer->PointData = PointIO->NewEmptyOutput(Context, PCGEx::OutputPointsLabel);
			Layer->Points = &Layer->PointData->GetMutablePoints();
		}

		FPCGPoint& PointCopy = Layer->NewPoint(Point);
		Layer->PointData->Metadata->InitializeOnSet(PointCopy.MetadataEntry);

		int32 SubIndex = 0;
		for (const PCGExPartition::FRule& Rule : Context->Rules)
		{
			if (Rule.KeyAttribute) { Rule.KeyAttribute->SetValue(PointCopy.MetadataEntry, LayerKeys[SubIndex]); }
			SubIndex++;
		}

		LayerKeys.Empty();
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->bSplitOutput)
		{
			if (Context->AsyncProcessingCurrentPoints(Initialize, DistributePoint))
			{
				Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			}
		}
		else
		{
			if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
			{
				Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			}
		}
	}

	if (Context->IsDone())
	{
		if (!Context->bSplitOutput) { Context->MainPoints->OutputTo(Context); }
		Context->RootPartitionLayer->Flush();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

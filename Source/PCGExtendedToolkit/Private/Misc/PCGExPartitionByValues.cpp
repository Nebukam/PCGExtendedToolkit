// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPartitionByValues.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionByValues"

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const { return MakeShared<FPCGExPartitionByValuesElement>(); }

PCGExIO::EInitMode UPCGExPartitionByValuesSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NoOutput; }

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
		FPCGExPartitionRuleDescriptor& DescriptorCopy = Context->RulesDescriptors.Add_GetRef(Descriptor);
		if (Descriptor.bWriteKey && !PCGEx::IsValidName(Descriptor.KeyAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedAttributeName", "Key Partition name %s is invalid."));
			DescriptorCopy.bWriteKey = false;
		}
	}

	Context->Depth = Context->RulesDescriptors.Num();

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

	auto Initialize = [&](const UPCGExPointIO* PointIO)
	{
		FWriteScopeLock WriteLock(Context->RulesLock);
		Context->Rules.Empty();

		for (FPCGExPartitionRuleDescriptor& Descriptor : Context->RulesDescriptors)
		{
			PCGExPartition::FRule& NewRule = Context->Rules.Emplace_GetRef(Descriptor);
			if (!NewRule.Validate(PointIO->In))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedRule", "Rule %s invalid on input %s."));
				Context->Rules.Pop();
			}
		}
	};

	auto ProcessPoint = [&](const FPCGPoint& Point, const int32 Index, const UPCGExPointIO* PointIO)
	{
		TArray<int64> KeyValues;
		FWriteScopeLock ReadLock(Context->RulesLock);

		PCGExPartition::FLayer* Layer = &Context->RootLayer;
		int32 CurrentDepth = 1;

		for (PCGExPartition::FRule& Rule : Context->Rules)
		{
			const int64 KeyValue = Rule.Filter(Point);
			const bool bLastDepth = Context->Depth == CurrentDepth;

			KeyValues.Add(KeyValue);

			Layer = Layer->GetLayer(KeyValue, PointIO->In, Context->LayerBuffer);


			if (bLastDepth)
			{
				if (!Layer->PointData)
				{
					Layer->PointData = NewObject<UPCGPointData>();
					Layer->PointData->InitializeFromData(PointIO->In);

					FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
					OutputRef.Data = Layer->PointData;
					OutputRef.Pin = PCGEx::OutputPointsLabel;
				}

				FPCGPoint* PointPtr = nullptr;

				FPCGPoint& PointCopy = Layer->PointData->GetMutablePoints().Add_GetRef(Point);
				PointPtr = &PointCopy;

				Layer->PointData->Metadata->InitializeOnSet(PointPtr->MetadataEntry);
				int32 SubIndex = 0;
				for (int i = 0; i < Context->Depth; i++)
				{
					if (const PCGExPartition::FRule& WRule = Context->Rules[i];
						WRule.RuleDescriptor->bWriteKey)
					{
						FPCGMetadataAttribute<int64>* KeyAttribute = Layer->PointData->Metadata->FindOrCreateAttribute<int64>(WRule.RuleDescriptor->KeyAttributeName, 0, false);
						KeyAttribute->SetValue(PointPtr->MetadataEntry, KeyValues[i]);
						SubIndex++;
					}
				}
			}

			CurrentDepth++;
		}

		KeyValues.Empty();
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize, !Context->bDoAsyncProcessing))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->LayerBuffer.Empty();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

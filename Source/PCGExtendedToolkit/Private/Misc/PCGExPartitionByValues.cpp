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

UPCGExPartitionLayer::~UPCGExPartitionLayer()
{
	TArray<int64> Keys;
	SubLayers.GetKeys(Keys);
	for (const int64 Key : Keys) { delete *SubLayers.Find(Key); }
	SubLayers.Empty();
	PointData = nullptr;
	Points = nullptr;
}

void UPCGExPartitionByValuesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExPartitionRuleDescriptor& Descriptor : PartitionRules) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UPCGExPartitionByValuesSettings::GetMainPointsInputAcceptMultipleData() const { return false; }

#endif

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const
{
	return MakeShared<FPCGExPartitionByValuesElement>();
}

PCGExPointIO::EInit UPCGExPartitionByValuesSettings::GetPointOutputInitMode() const
{
	return bSplitOutput ? PCGExPointIO::EInit::NoOutput : PCGExPointIO::EInit::DuplicateInput;
}

FPCGExSplitByValuesContext::~FPCGExSplitByValuesContext()
{
	delete RootPartitionLayer;
}

void FPCGExSplitByValuesContext::PrepareForPoints(const FPCGExPointIO& PointIO)
{
	FWriteScopeLock WriteLock(RulesLock);

	Rules.Empty();

	for (FPCGExPartitionRuleDescriptor& Descriptor : RulesDescriptors)
	{
		PCGExPartition::FRule& NewRule = Rules.Emplace_GetRef(Descriptor);
		if (!NewRule.Validate(PointIO.GetIn()))
		{
			//PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MalformedRule", "Rule %s invalid on input %s."));
			Rules.Pop();
			continue;
		}

		if (Descriptor.bWriteKey)
		{
			NewRule.KeyAttribute = PointIO.GetOut()->Metadata->FindOrCreateAttribute<int64>(Descriptor.KeyAttributeName, 0, false, true);
		}
		else
		{
			NewRule.KeyAttribute = nullptr;
		}
	}
}

void FPCGExSplitByValuesContext::PrepareForPointsWithMetadataEntries(FPCGExPointIO& PointIO)
{
	PointIO.BuildMetadataEntries();
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
	Context->RootPartitionLayer = new UPCGExPartitionLayer();
	return Context;
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

	////

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](FPCGExPointIO& PointIO)
		{
			if (Context->bSplitOutput) { Context->PrepareForPoints(PointIO); }
			else { Context->PrepareForPointsWithMetadataEntries(PointIO); }
		};

		if (Context->bSplitOutput)
		{
			// Split output
			auto DistributePoint = [&](const int32 PointIndex, const FPCGExPointIO& PointIO)
			{
				const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);

				TArray<int64> LayerKeys;
				UPCGExPartitionLayer* Layer = Context->RootPartitionLayer;

				for (PCGExPartition::FRule& Rule : Context->Rules)
				{
					const int64 LayerKey = Rule.Filter(Point);
					LayerKeys.Add(LayerKey);
					Layer = Layer->GetLayer(LayerKey);
				}

				if (!Layer->Points)
				{
					FWriteScopeLock WriteLock(Context->ContextLock);

					Layer->PointData = PointIO.NewEmptyOutput(Context, PCGEx::OutputPointsLabel);
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

			if (Context->ProcessCurrentPoints(Initialize, DistributePoint))
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
		}
		else
		{
			// Only write partition values
			auto ProcessPoint = [&](const int32 PointIndex, const FPCGExPointIO& PointIO)
			{
				const FPCGPoint& Point = PointIO.GetOutPoint(PointIndex);
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

			if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
		}
	}

	if (Context->IsDone())
	{
		if (!Context->bSplitOutput) { Context->MainPoints->OutputTo(Context); }
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPartitionByValues.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Misc/PCGExFilter.h"
#include <mutex>

#define LOCTEXT_NAMESPACE "PCGExDummyElement"

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExPartitionByValuesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSplitByAttribute", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a MERGE before.");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const { return MakeShared<FPCGExPartitionByValuesElement>(); }

PCGEx::EIOInit UPCGExPartitionByValuesSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

FPCGContext* FPCGExPartitionByValuesElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExSplitByValuesContext* Context = new FPCGExSplitByValuesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExPartitionByValuesElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);
	FPCGExSplitByValuesContext* Context = static_cast<FPCGExSplitByValuesContext*>(InContext);

	const UPCGExPartitionByValuesSettings* Settings = InContext->GetInputSettings<UPCGExPartitionByValuesSettings>();
	check(Settings);

	Context->PartitionKeyName = Settings->KeyAttributeName;
	Context->bWritePartitionKey = Settings->bWriteKeyToAttribute;
	Context->PartitionsMap.Empty();
	Context->PartitionRule = Settings->PartitioningRules;

	// ...
}


bool FPCGExPartitionByValuesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBucketEntryElement::Execute);

	FPCGExSplitByValuesContext* Context = static_cast<FPCGExSplitByValuesContext*>(InContext);

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
	{
		if (Context->Points.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		const UPCGExPartitionByValuesSettings* Settings = Context->GetInputSettings<UPCGExPartitionByValuesSettings>();
		check(Settings);

		if (Settings->bWriteKeyToAttribute && !PCGEx::Common::IsValidName(Settings->KeyAttributeName))
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MalformedAttributeName", "Output Attribute name is invalid."));
			return true;
		}

		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
	}

	auto ProcessAttribute = [&Context](int32 ReadIndex)
	{
		FPCGPoint InPoint = Context->CurrentIO->In->GetPoint(ReadIndex);
		auto ProcessPoints = [&Context, &InPoint](auto DummyValue)
		{
			using T = decltype(DummyValue);
			if (T OutValue = T{}; Context->PartitionRule.TryGetValue(InPoint.MetadataEntry, OutValue))
			{
				DistributePoint(Context, InPoint, OutValue);
			};
		};
		PCGMetadataAttribute::CallbackWithRightType(Context->PartitionRule.UnderlyingType, ProcessPoints);
	};

#define PCGEX_COMPARE__CASE(_ENUM, _ACCESSOR) case _ENUM : DistributePoint(Context, InPoint, InPoint._ACCESSOR); break;

	auto ProcessPointProperty = [&Context](int32 ReadIndex)
	{
		FPCGPoint InPoint = Context->CurrentIO->In->GetPoint(ReadIndex);
		switch (Context->PartitionRule.Selector.GetPointProperty())
		{
		PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE__CASE)
		default: ;
		}
	};

	auto ProcessPointExtraProperty = [&Context](int32 ReadIndex)
	{
		FPCGPoint InPoint = Context->CurrentIO->In->GetPoint(ReadIndex);

		switch (Context->PartitionRule.Selector.GetExtraProperty())
		{
		PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE__CASE)
		default: ;
		}
	};

	bool bProcessingAllowed = false;
	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			if (!Context->PartitionRule.Validate(Context->CurrentIO->In))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingAttribute", "Some inputs are missing the reference partition attribute, they will be omitted."));
				return false;
			}

			Context->Selection = Context->PartitionRule.Selector.GetSelection();
			bProcessingAllowed = true;
		}
	}

	auto Initialize = [&Context]()
	{
		Context->SetOperation(PCGEx::EOperation::ProcessingPoints);
	};

	if (true) // Async op
	{
		if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingPoints) || bProcessingAllowed)
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingPoints);
			switch (Context->Selection)
			{
			case EPCGAttributePropertySelection::Attribute:
				PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, ProcessAttribute);
				break;
			case EPCGAttributePropertySelection::PointProperty:
				PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, ProcessPointProperty);
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, ProcessPointExtraProperty);
				break;
			default: ;
			}
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
		}
	}
	else // Multithreaded (uncomment mutex!!)
	{
		if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingPoints) || bProcessingAllowed)
		{
			bool bProcessingDone = false;
			switch (Context->Selection)
			{
			case EPCGAttributePropertySelection::Attribute:
				bProcessingDone = PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessAttribute);
				break;
			case EPCGAttributePropertySelection::PointProperty:
				bProcessingDone = PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessPointProperty);
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				bProcessingDone = PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessPointExtraProperty);
				break;
			default: ;
			}
			if (bProcessingDone) { Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints); }
		}
	}


	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Partitions.OutputTo(Context, true);
		return true;
	}

	return false;
}

template <typename T>
void FPCGExPartitionByValuesElement::DistributePoint(
	FPCGExSplitByValuesContext* Context,
	FPCGPoint& Point,
	const T& InValue)
{
	const int64 Key = FPCGExFilter::Filter(InValue, Context->PartitionRule);
	PCGEx::FPointIO* Partition = nullptr;

	{
		//std::shared_lock<std::shared_mutex> Lock(Context->PartitionMutex); //Lock for read
		if (PCGEx::FPointIO** PartitionPtr = Context->PartitionsMap.Find(Key)) { Partition = *PartitionPtr; }
	}

	FPCGMetadataAttribute<int64>* KeyAttribute = nullptr;

	if (!Partition)
	{
		//std::unique_lock<std::shared_mutex> Lock(Context->PartitionMutex); //Lock for write

		Partition = &(Context->Partitions.Emplace_GetRef(*Context->CurrentIO, PCGEx::EIOInit::NewOutput));
		Context->PartitionsMap.Add(Key, Partition);

		if (Context->bWritePartitionKey)
		{
			KeyAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<int64>(Partition->Out->Metadata, Context->PartitionKeyName, 0);
			if (KeyAttribute) { Context->KeyAttributeMap.Add(Key, KeyAttribute); } //Cache attribute for this partition
		}
	}
	else
	{
		if (Context->bWritePartitionKey)
		{
			//std::shared_lock<std::shared_mutex> Lock(Context->PartitionMutex); //Lock for read
			FPCGMetadataAttribute<int64>** KeyAttributePtr = Context->KeyAttributeMap.Find(Key);
			KeyAttribute = KeyAttributePtr ? *KeyAttributePtr : nullptr;
		}
	}

	Partition->Out->GetMutablePoints().Add(Point);

	if (KeyAttribute)
	{
		Partition->Out->Metadata->InitializeOnSet(Point.MetadataEntry);
		KeyAttribute->SetValue(Point.MetadataEntry, Key);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_PROPERTY_CASE
#undef PCGEX_COMPARE_PROPERTY_CASE

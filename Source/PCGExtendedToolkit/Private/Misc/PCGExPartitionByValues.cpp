// Fill out your copyright notice in the Description page of Project Rules.

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "Misc/PCGExPartitionByValues.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Relational/PCGExFilter.h"

#define LOCTEXT_NAMESPACE "PCGExDummyElement"

namespace PCGExPartitionByValues
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExPartitionByValuesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSplitByAttribute", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExPartitionByValuesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExPartitionByValues::SourceLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "Input data to split into separate buckets. Note that input data will not be merged, which can lead to duplicate groups; if this is not desirable, merge the input beforehand.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPartitionByValuesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs multiple point buckets for each input data.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGExPartitionByValuesSettings::CreateElement() const
{
	return MakeShared<FPCGExPartitionByValuesElement>();
}

bool FPCGExPartitionByValuesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBucketEntryElement::Execute);

	const UPCGExPartitionByValuesSettings* Settings = Context->GetInputSettings<UPCGExPartitionByValuesSettings>();
	check(Settings);

	const FPCGExPartitioningRules BaseRules = Settings->PartitioningRules;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExPartitionByValues::SourceLabel);

	TArray<int> InSources;
	TArray<const UPCGPointData*> PerSourcePointData;
	TArray<FPCGExPartitioningRules> PerSourceRules;
	PerSourcePointData.Reserve(Sources.Num());

	// Safety checks...
	for (int i = 0; i < Sources.Num(); i++)
	{
		const FPCGTaggedData& Source = Sources[i];
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(Source.Data);

		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TargetMustBeSpatial", "Source must be Spatial data, found '{0}'"), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		const UPCGPointData* InPointData = InSpatialData->ToPointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CannotConvertToPoint", "Cannot source '{0}' into Point data"), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		FPCGExPartitioningRules Rules = FPCGExPartitioningRules(BaseRules);
		if (!Rules.CopyAndFixLast(InPointData))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExists", "Attribute '{0}' does not exist in source '{1}'"), FText::FromString(Rules.ToString()),FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		PerSourcePointData.Add(InPointData);
		PerSourceRules.Add(Rules);
		InSources.Add(i);
	}

	if (InSources.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoData", "Current inputs contains no partitionable data."));
		return true;
	}

	TMap<int64, UPCGPointData*> Partitions;
	FPCGExProcessingData ProcessingData = FPCGExProcessingData{};
	ProcessingData.Context = Context;
	ProcessingData.Partitions = &Partitions;
	if(Settings->bWriteKeyToAttribute)
	{
		TMap<int64, FPCGMetadataAttribute<int64>*> KeyAttributes;
		ProcessingData.OutAttributes = &KeyAttributes;
		ProcessingData.bWriteKeyToAttribute = true;
		ProcessingData.AttributeName = Settings->KeyAttributeName;
	}

	// Create a temp data holder to pre-process points and cache values		
	UPCGPointData* PointsBuffer = NewObject<UPCGPointData>();
	ProcessingData.PointsBuffer = &PointsBuffer->GetMutablePoints();

	for (int i = 0; i < PerSourcePointData.Num(); i++)
	{
		// Reset buckets
		Partitions.Reset();

		ProcessingData.Source = &Sources[InSources[i]];
		ProcessingData.InPointData = &PerSourcePointData[i];
		ProcessingData.Rules = &PerSourceRules[i];

		const UPCGPointData* InPointData = *ProcessingData.InPointData;
		const TArray<FPCGPoint> InPoints = InPointData->GetPoints();

		// Create a temp data holder to pre-process points and cache values.		
		// UPCGPointData* TempPointData = NewObject<UPCGPointData>();
		// TempPointData->InitializeFromData(ProcessingData.InPointData);
		// ProcessingData.PointsBuffer = &TempPointData->GetMutablePoints();

		// Branch out sampling behavior based on selection
		switch (ProcessingData.Rules->Selector.GetSelection())
		{
		case EPCGAttributePropertySelection::Attribute:
			AsyncPointAttributeProcessing(&ProcessingData);
			break;
		case EPCGAttributePropertySelection::PointProperty:
			AsyncPointPropertyProcessing(&ProcessingData);
			break;
		case EPCGAttributePropertySelection::ExtraProperty:
			AsyncPointPropertyProcessing(&ProcessingData);
			break;
		}
	}

	return true;
}

template <typename T>
void FPCGExPartitionByValuesElement::DistributePoint(const FPCGPoint& Point, const T& InValue, FPCGExProcessingData* Data)
{
	const int64 Key = FPCGExFilter::Filter(InValue, *Data->Rules);
	UPCGPointData** PartitionPtr = Data->Partitions->Find(Key);
	UPCGPointData* Partition;
	FPCGMetadataAttribute<int64>* KeyAttribute;

	if (PartitionPtr == nullptr)
	{
		// No partition exists with the filtered key, create one. 
		Partition = NewObject<UPCGPointData>();
		const UPCGPointData* InPointData = *Data->InPointData;
		Partition->InitializeFromData(InPointData);

		TArray<FPCGTaggedData>& Outputs = Data->Context->OutputData.TaggedData;
		Outputs.Add_GetRef(*Data->Source).Data = Partition;

		Data->Partitions->Add(Key, Partition);

		if(Data->bWriteKeyToAttribute)
		{
			KeyAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<int64>(Partition->Metadata, Data->AttributeName, 0);
			Data->OutAttributes->Add(Key, KeyAttribute);
			KeyAttribute->SetValue(Point.MetadataEntry, Key);
		}
		
	}
	else
	{
		// An existing partition already covers that key.
		Partition = *PartitionPtr;
		if(Data->bWriteKeyToAttribute)
		{
			KeyAttribute = *Data->OutAttributes->Find(Key);
			KeyAttribute->SetValue(Point.MetadataEntry, Key);
		}
	}

	Partition->GetMutablePoints().Add(Point);
}

void FPCGExPartitionByValuesElement::AsyncPointAttributeProcessing(FPCGExProcessingData* Data)
{
	auto ProcessPoints = [&Data](auto DummyValue)
	{
		using T = decltype(DummyValue);
		FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(Data->Rules);
		const UPCGPointData* InPointData = *Data->InPointData;
		FPCGAsync::AsyncPointProcessing(Data->Context, InPointData->GetPoints(), *Data->PointsBuffer, [&DummyValue, &Data, &Attribute](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			DistributePoint(InPoint, Attribute->GetValue(InPoint.MetadataEntry), Data);
			return false;
		});

		return true;
	};

	//TODO: Use attribute->GetTypeID() on copy/fix setting instead of creating an accessor just to get the underlying type
	const UPCGPointData* InPointData = *Data->InPointData;
	const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Data->Rules->Selector);

	EPCGMetadataTypes Type = static_cast<EPCGMetadataTypes>(Accessor->GetUnderlyingType());
	PCGMetadataAttribute::CallbackWithRightType(static_cast<int16>(Type), ProcessPoints);
}

#define PCGEX_PROPERTY_CASE(_ACCESSOR)\
FPCGAsync::AsyncPointProcessing(Data->Context, InPointData->GetPoints(), *Data->PointsBuffer, [&Data](const FPCGPoint& InPoint, FPCGPoint& OutPoint)\
{ DistributePoint(InPoint, InPoint._ACCESSOR, Data); return false; });

#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) case _ENUM : PCGEX_PROPERTY_CASE(_ACCESSOR) break;

void FPCGExPartitionByValuesElement::AsyncPointPropertyProcessing(FPCGExProcessingData* Data)
{
	const UPCGPointData* InPointData = *Data->InPointData;\
	switch (Data->Rules->Selector.GetPointProperty())
	{
	PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
	default: ;
	}
}

void FPCGExPartitionByValuesElement::AsyncPointExtraPropertyProcessing(FPCGExProcessingData* Data)
{
	const UPCGPointData* InPointData = *Data->InPointData;\
	switch (Data->Rules->Selector.GetExtraProperty())
	{
	PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
	default: ;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_PROPERTY_CASE
#undef PCGEX_COMPARE_PROPERTY_CASE

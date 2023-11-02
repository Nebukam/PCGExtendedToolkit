// Fill out your copyright notice in the Description page of Project Settings.

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "Relational/PCGExSplitByAttribute.h"
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

namespace PCGExDummy
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExSplitByAttribute::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSplitByAttribute", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSplitByAttribute::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExDummy::SourceLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "Input data to split into separate buckets. Note that input data will not be merged, which can lead to duplicate groups; if this is not desirable, merge the input beforehand.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSplitByAttribute::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs multiple point buckets for each input data.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGExSplitByAttribute::CreateElement() const
{
	return MakeShared<FPCGExBucketEntryElement>();
}

bool FPCGExBucketEntryElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBucketEntryElement::Execute);

	const UPCGExSplitByAttribute* Settings = Context->GetInputSettings<UPCGExSplitByAttribute>();
	check(Settings);

	const FPCGExBucketSettings BucketSettings = Settings->BucketSettings;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExDummy::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<int> InSources;
	TArray<const UPCGPointData*> SourcePointDatas;
	TArray<FPCGExBucketSettings> PerSourceBucketSettings;
	SourcePointDatas.Reserve(Sources.Num());

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

		FPCGExBucketSettings CurrentBucketSettings = FPCGExBucketSettings(BucketSettings);
		if (!CurrentBucketSettings.CopyAndFixLast(InPointData))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExists", "Attribute '{0}' does not exist in source '{1}'"), FText::FromString(CurrentBucketSettings.ToString()), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		SourcePointDatas.Add(InPointData);
		PerSourceBucketSettings.Add(CurrentBucketSettings);
		InSources.Add(i);
	}

	TMap<int64, UPCGPointData*> Buckets;
	FPCGExBucketProcessingData ProcessingData = FPCGExBucketProcessingData{};

	for (int i = 0; i < SourcePointDatas.Num(); i++)
	{

		// Reset buckets
		Buckets.Reset();
		
		ProcessingData.Context = Context;
		ProcessingData.Source = &Sources[InSources[i]];
		ProcessingData.InPointData = SourcePointDatas[i];
		ProcessingData.Settings = &PerSourceBucketSettings[i];
		ProcessingData.Buckets = &Buckets;
				
		const TArray<FPCGPoint> InPoints = ProcessingData.InPointData->GetPoints();
		const EPCGAttributePropertySelection Selection = ProcessingData.Settings->Selector.GetSelection();

		// Create a temp data holder to pre-process points and cache values.		
		UPCGPointData* TempPointData = NewObject<UPCGPointData>();
		TempPointData->InitializeFromData(ProcessingData.InPointData);
		ProcessingData.TempPoints = &TempPointData->GetMutablePoints();
		
		// Branch out sampling behavior based on selection
		if (Selection == EPCGAttributePropertySelection::Attribute)
		{
			AsyncPointAttributeProcessing(&ProcessingData);
		}
		else if (Selection == EPCGAttributePropertySelection::PointProperty)
		{
			AsyncPointPropertyProcessing(&ProcessingData);
		}
		else if (Selection == EPCGAttributePropertySelection::ExtraProperty)
		{
			AsyncPointPropertyProcessing(&ProcessingData);
		}

	}

	return true;
}

template <typename T>
void FPCGExBucketEntryElement::DistributePoint(const FPCGPoint& Point, const T& InValue, FPCGExBucketProcessingData* Data)
{
	const int64 Key = UPCGExFilter::Filter(InValue, *Data->Settings);
	UPCGPointData** BucketEntry = Data->Buckets->Find(Key);
	UPCGPointData* Bucket = nullptr;

	TArray<FPCGTaggedData>& Outputs = Data->Context->OutputData.TaggedData;

	if (BucketEntry == nullptr)
	{
		Bucket = NewObject<UPCGPointData>();
		Bucket->InitializeFromData(Data->InPointData);
		Outputs.Add_GetRef(*Data->Source).Data = Bucket;
		Data->Buckets->Add(Key, Bucket);
	}
	else
	{
		Bucket = *BucketEntry;
	}

	Bucket->GetMutablePoints().Add(Point);
}

void FPCGExBucketEntryElement::AsyncPointAttributeProcessing(FPCGExBucketProcessingData* Data)
{
	auto ComputeBucketID = [&Data](auto DummyValue)
	{
		using T = decltype(DummyValue);
		FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(Data->Settings);
		FPCGAsync::AsyncPointProcessing(Data->Context, Data->InPointData->GetPoints(), *Data->TempPoints, [&DummyValue, &Data, &Attribute](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			T Value = Attribute->GetValue(InPoint.MetadataEntry);
			DistributePoint(InPoint, Value, Data);
			return false;
		});

		return true;
	};

	TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data->InPointData, Data->Settings->Selector);

	EPCGMetadataTypes Type = static_cast<EPCGMetadataTypes>(Accessor->GetUnderlyingType());
	PCGMetadataAttribute::CallbackWithRightType(static_cast<int16>(Type), ComputeBucketID);
}

#define PCGEX_PROPERTY_CASE(_ACCESSOR)\
FPCGAsync::AsyncPointProcessing(Data->Context, Data->InPointData->GetPoints(), *Data->TempPoints, [&Data](const FPCGPoint& InPoint, FPCGPoint& OutPoint)\
{ DistributePoint(InPoint, InPoint._ACCESSOR, Data); return false; });

#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) case _ENUM : PCGEX_PROPERTY_CASE(_ACCESSOR) break;

void FPCGExBucketEntryElement::AsyncPointPropertyProcessing(FPCGExBucketProcessingData* Data)
{
	switch (Data->Settings->Selector.GetPointProperty())
	{
	PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
	default: ;
	}
}

void FPCGExBucketEntryElement::AsyncPointExtraPropertyProcessing(FPCGExBucketProcessingData* Data)
{
	switch (Data->Settings->Selector.GetExtraProperty())
	{
	PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
	default: ;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_PROPERTY_CASE
#undef PCGEX_COMPARE_PROPERTY_CASE

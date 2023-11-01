// Fill out your copyright notice in the Description page of Project Settings.

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "Relational/PCGExSplitByAttribute.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "PCGExAttributesUtils.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

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

	const FName InBucketAttributeName = Settings->BucketIdentifierAttributeName;
	const FName OutBucketAttributeName = Settings->OutBucketIdentifierAttributeName;
	const int64 FilterSize = Settings->FilterSize;
	const bool bWriteOutputBucketIdentifier = Settings->bWriteOutputBucketIdentifier;
	const double FilterSize_d = static_cast<double>(FilterSize);
	const double Upscale = Settings->Upscale;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExDummy::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<FPCGTaggedData> ValidSources;
	ValidSources.Reserve(Sources.Num());

	// Safety checks...
	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TargetMustBeSpatial", "Source must be Spatial data, found '{0}'"), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		const UPCGPointData* SourcePointData = SourceData->ToPointData(Context);
		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CannotConvertToPoint", "Cannot source '{0}' into Point data"), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		const bool bAttributeExists = SourcePointData->Metadata->HasAttribute(InBucketAttributeName);
		if (!bAttributeExists)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExists", "Attribute '{0}' does not exist in source '{1}'"), FText::FromString(InBucketAttributeName.ToString()), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		EPCGMetadataTypes AttributeType = EPCGMetadataTypes::Unknown;
		if (AttributeHelpers::TryGetAttributeType(SourcePointData->Metadata, InBucketAttributeName, AttributeType))
		{
			EPCGExTypeCategory AttributeCategory = AttributeHelpers::GetCategory(AttributeType);

			if (AttributeCategory == EPCGExTypeCategory::Unsupported ||
				AttributeCategory == EPCGExTypeCategory::Complex ||
				AttributeCategory == EPCGExTypeCategory::Composite)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeUnusable", "Attribute '{0}' type in source '{0}' cannot be used."), FText::FromString(InBucketAttributeName.ToString()), FText::FromString(Source.Data->GetClass()->GetName())));
				continue;
			}
		}
		else
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExists", "Attribute '{0}' does not exist in source data data '{1}'"), FText::FromString(InBucketAttributeName.ToString()), FText::FromString(Source.Data->GetClass()->GetName())));
			continue;
		}

		ValidSources.Add(Source);
	}

	for (const FPCGTaggedData& InSource : ValidSources)
	{
		const UPCGSpatialData* InData = Cast<UPCGSpatialData>(InSource.Data);
		const UPCGPointData* InPointData = InData->ToPointData(Context);
		const TArray<FPCGPoint> InPoints = InPointData->GetPoints();

		EPCGMetadataTypes AttributeType = EPCGMetadataTypes::Unknown;
		AttributeHelpers::TryGetAttributeType(InPointData->Metadata, InBucketAttributeName, AttributeType);
		EPCGExTypeCategory AttributeCategory = AttributeHelpers::GetCategory(AttributeType);

		// Prepare bucket processing data
		int TempReserve = FMath::Max(FMath::Min(InPoints.Num(), InPoints.Num() / 5), 5);
		TArray<FPCGExBucketEntry> BucketEntries; // Store entries
		TMap<FString, int32> StringBucketsMap; // Map BucketID (String) to entry index within Buckets
		TMap<int32, int32> NumericBucketsMap; // Map BucketID (Numeric) to entry index within Buckets
		BucketEntries.Reserve(TempReserve);
		StringBucketsMap.Reserve(TempReserve);
		NumericBucketsMap.Reserve(TempReserve);

		// Create a copy of the input data in order to cache values.
		// This cache is NOT added to the outputs, but will be the used to dispatch points into buckets later on.
		UPCGPointData* CachedPointData = NewObject<UPCGPointData>();
		CachedPointData->InitializeFromData(InPointData);
		TArray<FPCGPoint> CachedPoints = CachedPointData->GetMutablePoints();
		FPCGMetadataAttribute<int32>* BucketAttribute = CachedPointData->Metadata->FindOrCreateAttribute<int32>(OutBucketAttributeName, -1, false, true, true);


#pragma region Find buckets macros

		// Sets up typed attribute and expects the _BODY to compute a BucketID value based on numeric AttributeValue.
#define PCGEX_FINDBUCKET_NUM(_TYPE, _BODY, ...) \
		FPCGMetadataAttribute<_TYPE>* SampledAttribute_##_TYPE = CachedPointData->Metadata->FindOrCreateAttribute<_TYPE>(InBucketAttributeName);\
		auto FindBuckets_##_TYPE = [&BucketEntries, &NumericBucketsMap, BucketAttribute, Upscale, FilterSize_d, &SampledAttribute_##_TYPE](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint)	{ \
			OutPoint = SourcePoint; int32 BucketID = -1; \
			_TYPE AttributeValue = SampledAttribute_##_TYPE->GetValue(OutPoint.MetadataEntry);\
			_BODY \
			BucketAttribute->SetValue(OutPoint.MetadataEntry, FPCGExBucketEntryElement::ProcessBucket(BucketID, BucketEntries, NumericBucketsMap));\
			return true; }; \
		FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), CachedPoints, FindBuckets_##_TYPE);

		// Sets up typed attribute and expects the _BODY to compute a BucketID value based on string AttributeValue.
#define PCGEX_FINDBUCKET_STR(_TYPE, _BODY, ...) \
		FPCGMetadataAttribute<_TYPE>* SampledAttribute_##_TYPE = CachedPointData->Metadata->FindOrCreateAttribute<_TYPE>(InBucketAttributeName);\
		auto FindBuckets_##_TYPE = [&BucketEntries, &StringBucketsMap, BucketAttribute, Upscale, FilterSize_d, &SampledAttribute_##_TYPE](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint)	{ \
		OutPoint = SourcePoint; \
		_TYPE AttributeValue = SampledAttribute_##_TYPE->GetValue(OutPoint.MetadataEntry);\
		_BODY \
		BucketAttribute->SetValue(OutPoint.MetadataEntry, FPCGExBucketEntryElement::ProcessBucket(BucketID, BucketEntries, StringBucketsMap));\
		return true; }; \
		FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), CachedPoints, FindBuckets_##_TYPE);

		// Wraps up BucketID assignation
#define PCGEX_FINDBUCKET_BODY_IDDEF\
		const double Filtered = (Upscaled - FGenericPlatformMath::Fmod(Upscaled, FilterSize_d)) / FilterSize_d; \
		BucketID = static_cast<int>(Filtered);

		// Static cast to double * upscale
#define PCGEX_FINDBUCKET_VALUE(_VALUE)\
		const double Upscaled = static_cast<double>(_VALUE) * Upscale; PCGEX_FINDBUCKET_BODY_IDDEF

#pragma endregion

		if (AttributeCategory == EPCGExTypeCategory::Num)
		{
			if (AttributeType == EPCGMetadataTypes::Float) { PCGEX_FINDBUCKET_NUM(float, PCGEX_FINDBUCKET_VALUE(AttributeValue)) }
			else if (AttributeType == EPCGMetadataTypes::Double) { PCGEX_FINDBUCKET_NUM(double, PCGEX_FINDBUCKET_VALUE(AttributeValue)) }
			else if (AttributeType == EPCGMetadataTypes::Integer32) { PCGEX_FINDBUCKET_NUM(int32, PCGEX_FINDBUCKET_VALUE(AttributeValue)) }
			else if (AttributeType == EPCGMetadataTypes::Integer64) { PCGEX_FINDBUCKET_NUM(int64, PCGEX_FINDBUCKET_VALUE(AttributeValue)) }
			else if (AttributeType == EPCGMetadataTypes::Boolean) { PCGEX_FINDBUCKET_NUM(double, BucketID = static_cast<int64>(AttributeValue);) }
		}
		else if (AttributeCategory == EPCGExTypeCategory::Lengthy)
		{
			if (AttributeType == EPCGMetadataTypes::Vector2) { PCGEX_FINDBUCKET_NUM(FVector2D, PCGEX_FINDBUCKET_VALUE(AttributeValue.SquaredLength())) }
			else if (AttributeType == EPCGMetadataTypes::Vector) { PCGEX_FINDBUCKET_NUM(FVector, PCGEX_FINDBUCKET_VALUE(AttributeValue.SquaredLength())) }
			else if (AttributeType == EPCGMetadataTypes::Vector4) { PCGEX_FINDBUCKET_NUM(FVector4, PCGEX_FINDBUCKET_VALUE(static_cast<FVector>(AttributeValue).SquaredLength())) }
		}
		if (AttributeCategory == EPCGExTypeCategory::String)
		{
			if (AttributeType == EPCGMetadataTypes::Name) { PCGEX_FINDBUCKET_STR(FName, FString BucketID = AttributeValue.ToString();) }
			else if (AttributeType == EPCGMetadataTypes::String) { PCGEX_FINDBUCKET_STR(FString, FString BucketID = AttributeValue;) }
		}

		for (const FPCGExBucketEntry BucketEntry : BucketEntries)
		{
			UPCGPointData* OutPointData = NewObject<UPCGPointData>();
			OutPointData->InitializeFromData(InPointData);
			Outputs.Add_GetRef(InSource).Data = OutPointData;

			FPCGAsync::AsyncPointProcessing(
				Context, InPointData->GetPoints(), OutPointData->GetMutablePoints(),
				[BucketEntry, BucketAttribute](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint)
				{
					if (BucketAttribute->GetValue(SourcePoint.MetadataEntry) == BucketEntry.ID)
					{
						OutPoint = SourcePoint;
						return true;
					}
					else
					{
						return false;
					}
				}
			);
		}
	}

	return true;
}

template <typename T>
int FPCGExBucketEntryElement::ProcessBucket(T BucketID, TArray<FPCGExBucketEntry>& BucketEntries, TMap<T, int32>& StringBucketsMap)
{
	int* Index = StringBucketsMap.Find(BucketID);
	if (Index == nullptr)
	{
		int EntryIndex = BucketEntries.Num();
		FPCGExBucketEntry NewBucket = FPCGExBucketEntry{EntryIndex};
		BucketEntries.Add(NewBucket);
		StringBucketsMap.Add(BucketID, EntryIndex);
		return EntryIndex;
	}
	else
	{
		return *Index;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_FINDBUCKET_NUM
#undef PCGEX_FINDBUCKET_STR
#undef PCGEX_FINDBUCKET_BODY_IDDEF
#undef PCGEX_FINDBUCKET_VALUE

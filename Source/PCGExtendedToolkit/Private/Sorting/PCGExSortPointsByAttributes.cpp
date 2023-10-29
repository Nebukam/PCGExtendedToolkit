// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPointsByAttributes.h"
#include "PCGExAttributesUtils.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsByAttributesElement"

namespace PCGExSortPointsByAttributes
{
	const FName SourceLabel = TEXT("Source");
}

UPCGExSortPointsByAttributesSettings::UPCGExSortPointsByAttributesSettings()
{
	AttributesNames.Reserve(Attributes.Num());
	for(const FAttributeSortingInfos& AttInfos : Attributes)
	{
		AttributesNames.Add(AttInfos.AttributeName);
	}
}

#if WITH_EDITOR

FText UPCGExSortPointsByAttributesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSortPointsByAttributesTooltip", "Sort the source points according to specific rules.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSortPointsByAttributesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPointsByAttributes::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip",
	                                    "The order of the point in data will be changed, allowing to effectively rely on indices to perform index-bound operations, such as spline generation.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsByAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip",
	                                    "The source points will be sorted according to specified options.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGExSortPointsByAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGExSortPointsByAttributesElement>();
}

TArray<FName>& UPCGExSortPointsByAttributesSettings::GetAttributesNames() const
{
	return AttributesNames;
}

bool FPCGExSortPointsByAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	const UPCGExSortPointsByAttributesSettings* Settings = Context->GetInputSettings<UPCGExSortPointsByAttributesSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPointsByAttributes::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	
	const TArray<FAttributeSortingInfos>& OriginalAttributesSettings = Settings->Attributes;

	TArray<FName> SortableAttributeNames = Settings->GetAttributesNames();	
	TArray<FPCGExAttributeProxy> SortableAttributeFound;
	TArray<FPCGExAttributeProxy> SortableAttributeMissing;

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* SourcePointData = SourceData->ToPointData(Context);
		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog,
			         LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		AttributeHelpers::GetAttributesProxies(SourcePointData->Metadata, SortableAttributeNames, SortableAttributeFound, SortableAttributeMissing);


		if(SortableAttributeFound.Num() <= 0)
		{
			PCGE_LOG(Error, GraphAndLog,
					 LOCTEXT("CouldNotFindSortableAttributes", "Could not find any valid (existing) sortable attributes."));
			continue;
		}else if(SortableAttributeMissing.Num() > 0)
		{
			PCGE_LOG(Warning, GraphAndLog,
					 LOCTEXT("MissingAttributes", "Could not find any valid (existing) sortable attributes."));
		}
		
		// First, create a cache of valid attribute settings
		TArray<FAttributeSortingInfos> AttributesSettings;
		AttributesSettings.Reserve(OriginalAttributesSettings.Num());
		for (const FAttributeSortingInfos& Infos : OriginalAttributesSettings)
		{
			if (!SourcePointData->Metadata->HasAttribute(Infos.AttributeName)) { continue; }
			FPCGMetadataAttributeBase BaseAtt =SourcePointData->Metadata->GetAttributes()  
			AttributesSettings.Add(Infos);
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutPoints, [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			return true;
		});

		PCGExPointDataSorting::Sort(OutPoints, Settings->SortOver, Settings->SortDirection, Settings->SortOrder);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

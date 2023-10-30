// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPointsByAttributes.h"
#include "Sorting/PCGExPointSortHelpers.h"
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
	UniqueAttributeNames.Reserve(Attributes.Num());
	for (const FPCGExSortAttributeDetails& AttInfos : Attributes)
	{
		int Index = UniqueAttributeNames.AddUnique(AttInfos.AttributeName);
		if (Index != -1)
		{
			UniqueAttributeDetails.Add(AttInfos.AttributeName, AttInfos);
		}
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

bool UPCGExSortPointsByAttributesSettings::TryGetDetails(const FName Name, FPCGExSortAttributeDetails& OutDetails) const
{
	const FPCGExSortAttributeDetails* Found = UniqueAttributeDetails.Find(Name);
	if (Found == nullptr) { return false; }
	OutDetails = *Found;
	return true;
}

bool FPCGExSortPointsByAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	const UPCGExSortPointsByAttributesSettings* Settings = Context->GetInputSettings<UPCGExSortPointsByAttributesSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPointsByAttributes::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const TArray<FPCGExSortAttributeDetails>& OriginalAttributes = Settings->Attributes;

	const TArray<FName> UniqueAttributeNames = Settings->UniqueAttributeNames;
	TArray<FPCGExAttributeProxy> SortableAttributes;
	TArray<FName> MissingAttributesNames;
	TArray<FPCGExSortAttributeDetails> PerAttributeDetails;
	PerAttributeDetails.Reserve(UniqueAttributeNames.Num());

	for (const FPCGTaggedData& Source : Sources)
	{
		SortableAttributes.Reset();
		MissingAttributesNames.Reset();
		PerAttributeDetails.Reset();

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

		AttributeHelpers::GetAttributesProxies(SourcePointData->Metadata, UniqueAttributeNames, SortableAttributes, MissingAttributesNames);

		for (int i = 0; i < SortableAttributes.Num(); i++)
		{
			if (!PCGExPointSortHelpers::IsSortable(SortableAttributes[i].Type))
			{
				SortableAttributes.RemoveAt(i);
				i--;
			}
		}

		if (SortableAttributes.Num() <= 0)
		{
			PCGE_LOG(Error, GraphAndLog,
			         LOCTEXT("CouldNotFindSortableAttributes", "Could not find any existing or sortable attributes."));
			continue;
		}

		if (MissingAttributesNames.Num() > 0)
		{
			PCGE_LOG(Warning, GraphAndLog,
			         LOCTEXT("MissingAttributes", "Some attributes are missing and won't be processed."));
		}

		for (int i = 0; i < SortableAttributes.Num(); i++)
		{
			FPCGExAttributeProxy Proxy = SortableAttributes[i];
			FPCGExSortAttributeDetails Details;
			if (Settings->TryGetDetails(Proxy.Attribute->Name, Details))
			{
				PerAttributeDetails.Add(Details);
			}
			else
			{
				SortableAttributes.RemoveAt(i);
				i--;
			}
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

		PCGExPointSortHelpers::Sort(OutPoints, SortableAttributes, PerAttributeDetails, Settings->SortDirection);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

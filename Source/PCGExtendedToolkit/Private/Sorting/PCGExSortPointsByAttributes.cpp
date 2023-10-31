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
#include "Elements/SMInstance/SMInstanceElementDetailsInterface.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsByAttributesElement"

namespace PCGExSortPointsByAttributes
{
	const FName SourceLabel = TEXT("Source");
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

bool FPCGExSortPointsByAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	const UPCGExSortPointsByAttributesSettings* Settings = Context->GetInputSettings<UPCGExSortPointsByAttributesSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPointsByAttributes::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const TArray<FPCGExSortAttributeDetails>& OriginalAttributes = Settings->SortOver;

	TArray<FName> UniqueNames, MissingNames;
	TMap<FName, FPCGExSortAttributeDetails> DetailsMap;
	BuildUniqueAttributeList(OriginalAttributes, UniqueNames, DetailsMap);
	
	TArray<FPCGExAttributeProxy> ExistingAttributes;
	TArray<FPCGExSortAttributeDetails> PerAttributeDetails;
	PerAttributeDetails.Reserve(UniqueNames.Num());

	for (const FPCGTaggedData& Source : Sources)
	{
		ExistingAttributes.Reset();
		MissingNames.Reset();
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

		AttributeHelpers::GetAttributesProxies(SourcePointData->Metadata, UniqueNames, ExistingAttributes, MissingNames);

		for (int i = 0; i < ExistingAttributes.Num(); i++)
		{
			FPCGExAttributeProxy Proxy = ExistingAttributes[i];
			FPCGExSortAttributeDetails Details;
			if (!PCGExPointSortHelpers::IsSortable(ExistingAttributes[i].Type) ||
				TryGetDetails(Proxy.Attribute->Name, DetailsMap, Details))
			{
				PerAttributeDetails.Add(Details);
			}
			else
			{
				ExistingAttributes.RemoveAt(i);
				i--;
			}
		}

		if (ExistingAttributes.Num() <= 0)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CouldNotFindSortableAttributes", "Could not find any existing or sortable attributes."));
			continue;
		}

		if (MissingNames.Num() > 0)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingAttributes", "Some attributes are missing and won't be processed."));
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

		PCGExPointSortHelpers::Sort(OutPoints, ExistingAttributes, PerAttributeDetails, Settings->SortDirection);
	}

	return true;
}

void FPCGExSortPointsByAttributesElement::BuildUniqueAttributeList(const TArray<FPCGExSortAttributeDetails>& SettingsDetails, TArray<FName>& OutUniqueNames, TMap<FName, FPCGExSortAttributeDetails>& OutUniqueDetails)
{	
	OutUniqueNames.Reset();
	OutUniqueDetails.Reset();
	for (const FPCGExSortAttributeDetails& AttInfos : SettingsDetails)
	{
		int Index = OutUniqueNames.AddUnique(AttInfos.AttributeName);
		if (Index != -1)
		{
			OutUniqueDetails.Add(AttInfos.AttributeName, AttInfos);
		}
	}
}

bool FPCGExSortPointsByAttributesElement::TryGetDetails(const FName Name, TMap<FName, FPCGExSortAttributeDetails>& InMap, FPCGExSortAttributeDetails& OutDetails)
{
	const FPCGExSortAttributeDetails* Found = InMap.Find(Name);
	if (Found == nullptr) { return false; }
	OutDetails = *Found;
	return true;
}

#undef LOCTEXT_NAMESPACE

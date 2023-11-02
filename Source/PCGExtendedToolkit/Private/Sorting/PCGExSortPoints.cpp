// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPoints.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Sorting/PCGExCompare.h"

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

	const TArray<FPCGExSortSettings>& DesiredSelectorSettings = Settings->SortOver;
	ESortDirection SortDirection = Settings->SortDirection;
	TArray<FPCGExSortSettings> FixedDesiredAttributeSelectors;
	FixedDesiredAttributeSelectors.Reserve(DesiredSelectorSettings.Num());

	if (DesiredSelectorSettings.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("Empty", "No attributes to sort over."));
		return true; // Skip execution
	}

	TArray<FPCGExSortSettings> SortSelectorSettings;
	TArray<TUniquePtr<const IPCGAttributeAccessor>> Accessors;
	TArray<TUniquePtr<const IPCGAttributeAccessorKeys>> Keys;
	SortSelectorSettings.Reserve(DesiredSelectorSettings.Num());
	Accessors.Reserve(DesiredSelectorSettings.Num());

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(Source.Data);

		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* InPointData = InSpatialData->ToPointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		SortSelectorSettings.Reset();
		Accessors.Reset();
		Keys.Reset();

		for (const FPCGExSortSettings& SelectorSettings : DesiredSelectorSettings)
		{
			FPCGExSortSettings Selector = FPCGExSortSettings(SelectorSettings);

			if (!Selector.CopyAndFixLast(InPointData)) { continue; }

			if (Selector.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				//TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector);
				//TUniquePtr<const IPCGAttributeAccessorKeys> Key = PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, Selector.Selector);

				Accessors.Add(PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector));
				Keys.Add(PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, Selector.Selector));

				if (!Accessors.Last().IsValid())
				{
					Accessors.Pop();
					Keys.Pop();
					continue;
				}
			}
			else
			{
				Accessors.Add(nullptr);
				Keys.Add(nullptr);
			}

			SortSelectorSettings.Add(Selector);
		}

		if (SortSelectorSettings.Num() <= 0)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidSortSettings", "Invalid sort settings. Make sure attributes exist."));
			continue;
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(InPointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		// Copy original points
		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			return true;
		});


		auto Compare = [](auto DummyValue, const FPCGExSortSettings* CurrentSettings, const FPCGPoint& PtA, const FPCGPoint& PtB, const int32 Index) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(CurrentSettings);
			return FPCGExCompare::Compare(Attribute->GetValue(PtA.MetadataEntry), Attribute->GetValue(PtB.MetadataEntry), CurrentSettings->Tolerance, CurrentSettings->ComponentSelection);
		};

		OutPoints.Sort([&SortSelectorSettings, &Accessors, &Keys, &SortDirection, Compare](const FPCGPoint& A, const FPCGPoint& B)
		{
#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) \
case _ENUM : Result = FPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, CurrentSettings->Tolerance, CurrentSettings->ComponentSelection); break;

			int Result = 0;
			for (int i = 0; i < SortSelectorSettings.Num(); i++)
			{
				const FPCGExSortSettings* CurrentSettings = &SortSelectorSettings[i];

				EPCGAttributePropertySelection Sel = CurrentSettings->Selector.GetSelection();
				switch (Sel)
				{
				case EPCGAttributePropertySelection::Attribute:
					Result = PCGMetadataAttribute::CallbackWithRightType(Accessors[i]->GetUnderlyingType(), Compare, CurrentSettings, A, B, i);
					break;
				case EPCGAttributePropertySelection::PointProperty:
					// Compare properties
					switch (CurrentSettings->Selector.GetPointProperty())
					{
					PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (CurrentSettings->Selector.GetExtraProperty())
					{
					PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				default: ;
				}

				if (Result != 0) { break; }
			}

			if (SortDirection == ESortDirection::Ascending) { Result *= -1; }
			return Result <= 0;
		});
	}

	return true;
}

#undef PCGEX_COMPARE_PROPERTY
#undef PCGEX_COMPARE_PROPERTY_CASE

#undef LOCTEXT_NAMESPACE

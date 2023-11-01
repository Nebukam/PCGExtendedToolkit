// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPointsByAttributes.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
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
	TArray<FPCGExSortSettings> FixedDesiredAttributeSelectors;
	FixedDesiredAttributeSelectors.Reserve(DesiredSelectorSettings.Num());

	if (DesiredSelectorSettings.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("Empty", "No attributes to sort over."));
		return true; // Skip execution
	}

	TArray<FPCGExSortSelector> SortSelectors;
	TArray<TUniquePtr<const IPCGAttributeAccessor>> Accessors;
	SortSelectors.Reserve(DesiredSelectorSettings.Num());
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

		SortSelectors.Reset();
		Accessors.Reset();
		for (FPCGExSortSettings SelectorSettings : DesiredSelectorSettings)
		{
			FPCGExSortSelector Selector = SelectorSettings.CopyAndFixLast(InPointData);
			if (Selector.IsValid())
			{
				SortSelectors.Add(Selector);
				Accessors.Add(PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector));
			}
		}

		if (SortSelectors.Num() <= 0)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidSortSettings", "Invalid sort settings."));
			continue;
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(InPointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			return true;
		});

		FPCGExSortSelector* CurrentSelector = nullptr;

		auto CompareAttribute = [&CurrentSelector](auto DummyValue, const FPCGPoint& A, const FPCGPoint& B) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(CurrentSelector->Attribute);
			return UPCGExCompare::Compare(Attribute->GetValue(A.MetadataEntry), Attribute->GetValue(B.MetadataEntry), CurrentSelector->Tolerance, CurrentSelector->SortComponent);
		};

		OutPoints.Sort([CompareAttribute, &SortSelectors, &Accessors, &CurrentSelector](const FPCGPoint& A, const FPCGPoint& B)
		{
			int Result = 0;
			for (int i = 0; i < SortSelectors.Num(); i++)
			{
				CurrentSelector = &SortSelectors[i];
				if (CurrentSelector->Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
				{
#define PCGED_COMPARE_PROPERTY(_ACCESSOR)\
					Result = UPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, CurrentSelector->Tolerance, CurrentSelector->SortComponent);

					// Compare
					switch (CurrentSelector->Selector.GetPointProperty())
					{
					case EPCGPointProperties::Density: PCGED_COMPARE_PROPERTY(Density)
						break;
					case EPCGPointProperties::BoundsMin: PCGED_COMPARE_PROPERTY(BoundsMin)
						break;
					case EPCGPointProperties::BoundsMax: PCGED_COMPARE_PROPERTY(BoundsMax)
						break;
					case EPCGPointProperties::Extents: PCGED_COMPARE_PROPERTY(GetExtents())
						break;
					case EPCGPointProperties::Color: PCGED_COMPARE_PROPERTY(Color)
						break;
					case EPCGPointProperties::Position: PCGED_COMPARE_PROPERTY(Transform.GetLocation())
						break;
					case EPCGPointProperties::Rotation: PCGED_COMPARE_PROPERTY(Transform.Rotator())
						break;
					case EPCGPointProperties::Scale: PCGED_COMPARE_PROPERTY(Transform.GetScale3D())
						break;
					case EPCGPointProperties::Transform: PCGED_COMPARE_PROPERTY(Transform)
						break;
					case EPCGPointProperties::Steepness: PCGED_COMPARE_PROPERTY(Steepness)
						break;
					case EPCGPointProperties::LocalCenter: PCGED_COMPARE_PROPERTY(GetLocalCenter())
						break;
					case EPCGPointProperties::Seed: PCGED_COMPARE_PROPERTY(Seed)
						break;
					default: ;
					}
				}
				else
				{
					Result = PCGMetadataAttribute::CallbackWithRightType(Accessors[i]->GetUnderlyingType(), CompareAttribute, A, B);
				}

				if (Result != 0) { break; }
			}
			return Result <= 0;
		});
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

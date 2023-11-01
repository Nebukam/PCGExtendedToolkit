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
	TArray<FPCGExSortSettings> FixedDesiredAttributeSelectors;
	FixedDesiredAttributeSelectors.Reserve(DesiredSelectorSettings.Num());

	if (DesiredSelectorSettings.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("Empty", "No attributes to sort over."));
		return true; // Skip execution
	}

	TArray<FPCGExSortSelector> SortSelectors;
	TArray<TUniquePtr<const IPCGAttributeAccessor>> Accessors;
	TArray<TUniquePtr<const IPCGAttributeAccessorKeys>> Keys;
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

		for (const FPCGExSortSettings& SelectorSettings : DesiredSelectorSettings)
		{
			FPCGExSortSelector Selector = MakeSelectorFromSettings(SelectorSettings, InPointData);

			if (!Selector.IsValid(InPointData)) { continue; }

			if (Selector.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				//TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector);
				//TUniquePtr<const IPCGAttributeAccessorKeys> Key = PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, Selector.Selector);

				Accessors.Add(PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector));
                				
				if (!Accessors.Last().IsValid())
				{
					Accessors.Pop();
					continue;
				}

			}else
			{
				Accessors.Add(nullptr);
			}

			SortSelectors.Add(Selector);
		}

		if (SortSelectors.Num() <= 0)
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


		auto Compare = [](auto DummyValue, const FPCGExSortSelector* CurrentSelector, const FPCGPoint& PtA, const FPCGPoint& PtB) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(CurrentSelector->Attribute);
			return UPCGExCompare::Compare(Attribute->GetValue(PtA.MetadataEntry), Attribute->GetValue(PtB.MetadataEntry), CurrentSelector->Tolerance, CurrentSelector->SortComponent);
		};

		OutPoints.Sort([&SortSelectors, &Accessors, Compare](const FPCGPoint& A, const FPCGPoint& B)
		{
			int Result = 0;
			for (int i = 0; i < SortSelectors.Num(); i++)
			{
				const FPCGExSortSelector* CurrentSelector = &SortSelectors[i];

				EPCGAttributePropertySelection Sel = CurrentSelector->Selector.GetSelection();
				switch (Sel)
				{
				case EPCGAttributePropertySelection::Attribute:
					Result = PCGMetadataAttribute::CallbackWithRightType(Accessors[i]->GetUnderlyingType(), Compare, CurrentSelector, A, B);
					break;
				case EPCGAttributePropertySelection::PointProperty:
#define PCGEX_COMPARE_PROPERTY(_ACCESSOR)\
					Result = UPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, CurrentSelector->Tolerance, CurrentSelector->SortComponent);

					// Compare
					switch (CurrentSelector->Selector.GetPointProperty())
					{
					case EPCGPointProperties::Density: PCGEX_COMPARE_PROPERTY(Density)
						break;
					case EPCGPointProperties::BoundsMin: PCGEX_COMPARE_PROPERTY(BoundsMin)
						break;
					case EPCGPointProperties::BoundsMax: PCGEX_COMPARE_PROPERTY(BoundsMax)
						break;
					case EPCGPointProperties::Extents: PCGEX_COMPARE_PROPERTY(GetExtents())
						break;
					case EPCGPointProperties::Color: PCGEX_COMPARE_PROPERTY(Color)
						break;
					case EPCGPointProperties::Position: PCGEX_COMPARE_PROPERTY(Transform.GetLocation())
						break;
					case EPCGPointProperties::Rotation: PCGEX_COMPARE_PROPERTY(Transform.Rotator())
						break;
					case EPCGPointProperties::Scale: PCGEX_COMPARE_PROPERTY(Transform.GetScale3D())
						break;
					case EPCGPointProperties::Transform: PCGEX_COMPARE_PROPERTY(Transform)
						break;
					case EPCGPointProperties::Steepness: PCGEX_COMPARE_PROPERTY(Steepness)
						break;
					case EPCGPointProperties::LocalCenter: PCGEX_COMPARE_PROPERTY(GetLocalCenter())
						break;
					case EPCGPointProperties::Seed: PCGEX_COMPARE_PROPERTY(Seed)
						break;
					default: ;
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (CurrentSelector->Selector.GetExtraProperty())
					{
					case EPCGExtraProperties::Index: PCGEX_COMPARE_PROPERTY(MetadataEntry)
						break;
					default: ;
					}
					break;
				default: ;
				}

				if (Result != 0) { break; }
			}
			return Result <= 0;
		});
	}

	return true;
}

#undef PCGEX_COMPARE_PROPERTY

#undef LOCTEXT_NAMESPACE

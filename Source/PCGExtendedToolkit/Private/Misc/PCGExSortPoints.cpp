// Fill out your copyright notice in the Description page of Project Settings.


#include "Misc/PCGExSortPoints.h"
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
#include "Misc/PCGExCompare.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsByAttributesElement"

namespace PCGExSortPoints
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR

FText UPCGExSortPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSortPointsByAttributesTooltip", "Sort the source points according to specific rules.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPoints::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip",
	                                    "The order of the point in data will be changed, allowing to effectively rely on indices to perform index-bound operations, such as spline generation.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsSettings::OutputPinProperties() const
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

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const
{
	return MakeShared<FPCGExSortPointsElement>();
}

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	const UPCGExSortPointsSettings* Settings = Context->GetInputSettings<UPCGExSortPointsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPoints::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const TArray<FPCGExSortRule>& DesiredRules = Settings->Rules;
	EPCGExSortDirection SortDirection = Settings->SortDirection;

	if (DesiredRules.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("Empty", "No attributes to sort over."));
		return true; // Skip execution
	}

	TArray<FPCGExSortRule> Rules;

	TArray<TUniquePtr<const IPCGAttributeAccessor>> Accessors;
	TArray<TUniquePtr<const IPCGAttributeAccessorKeys>> Keys;
	Rules.Reserve(DesiredRules.Num());
	Accessors.Reserve(DesiredRules.Num());

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

		Rules.Reset();
		Accessors.Reset();
		Keys.Reset();

		for (const FPCGExSortRule& DesiredRule : DesiredRules)
		{
			FPCGExSortRule Rule = FPCGExSortRule(DesiredRule);

			if (!Rule.CopyAndFixLast(InPointData)) { continue; }

			if (Rule.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				//TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Selector.Selector);
				//TUniquePtr<const IPCGAttributeAccessorKeys> Key = PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, Selector.Selector);

				Accessors.Add(PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, Rule.Selector));
				Keys.Add(PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, Rule.Selector));

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

			Rules.Add(Rule);
		}

		if (Rules.Num() <= 0)
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

		auto Compare = [](auto DummyValue, const FPCGExSortRule* Rule, const FPCGPoint& PtA, const FPCGPoint& PtB, const int32 Index) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(Rule);
			return FPCGExCompare::Compare(Attribute->GetValue(PtA.MetadataEntry), Attribute->GetValue(PtB.MetadataEntry), Rule->Tolerance, Rule->ComponentSelection);
		};

		OutPoints.Sort([&Rules, &Accessors, &SortDirection, Compare](const FPCGPoint& A, const FPCGPoint& B)
		{
#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) \
case _ENUM : Result = FPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, CurrentRule->Tolerance, CurrentRule->ComponentSelection); break;

			int Result = 0;
			for (int i = 0; i < Rules.Num(); i++)
			{
				const FPCGExSortRule* CurrentRule = &Rules[i];
				switch (CurrentRule->Selector.GetSelection())
				{
				case EPCGAttributePropertySelection::Attribute:
					Result = PCGMetadataAttribute::CallbackWithRightType(Accessors[i]->GetUnderlyingType(), Compare, CurrentRule, A, B, i);
					break;
				case EPCGAttributePropertySelection::PointProperty:
					switch (CurrentRule->Selector.GetPointProperty())
					{
					PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (CurrentRule->Selector.GetExtraProperty())
					{
					PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				default: ;
				}

				if (Result != 0) { break; }
			}

			if (SortDirection == EPCGExSortDirection::Ascending) { Result *= -1; }
			return Result <= 0;
		});
	}

	return true;
}

#undef PCGEX_COMPARE_PROPERTY
#undef PCGEX_COMPARE_PROPERTY_CASE

#undef LOCTEXT_NAMESPACE

// Fill out your copyright notice in the Description page of Project Settings.


#include "Misc/PCGExSortPoints.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Misc/PCGExCompare.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsByAttributesElement"

namespace PCGExSortPoints
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR

#pragma region UPCGSettings interface

FText UPCGExSortPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSortPointsByAttributesTooltip", "Sort the source points according to specific rules.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPoints::SourceLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "The order of the point in data will be changed, allowing to effectively rely on indices to perform index-bound operations, such as spline generation.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "The source points will be sorted according to specified options.");
#endif // WITH_EDITOR

	return PinProperties;
}

#pragma endregion

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const
{
	return MakeShared<FPCGExSortPointsElement>();
}

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	const UPCGExSortPointsSettings* Settings = Context->GetInputSettings<UPCGExSortPointsSettings>();
	check(Settings);

	const TArray<FPCGExSortRule>& DesiredRules = Settings->Rules;

	if (DesiredRules.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSortParams", "No attributes to sort over."));
		return true;
	}

	PCGEx::FPointIOGroup<PCGEx::FPointIO> Points = PCGEx::FPointIOGroup<PCGEx::FPointIO>(Context, PCGExSortPoints::SourceLabel, PCGEx::EInitOutput::DuplicateInput);

	if (Points.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid or empty input data"));
		return true;
	}

	//

	EPCGExSortDirection SortDirection = Settings->SortDirection;
	TArray<FPCGExSortRule> Rules;

	Points.ForEach(Context, [&Context, &DesiredRules, &Rules, &SortDirection, this](PCGEx::FPointIO* POI, const int32)
	{
		//POI->ForwardPoints(Context);
		if (!BuildRulesForPoints(POI->Out, DesiredRules, Rules))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidSortSettings", "Invalid sort settings. Make sure attributes exist."));
			return;
		}

		auto Compare = [](auto DummyValue, const FPCGExSortRule* Rule, const FPCGPoint& PtA, const FPCGPoint& PtB) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Rule->Attribute);
			return FPCGExCompare::Compare(Attribute->GetValueFromItemKey(PtA.MetadataEntry), Attribute->GetValueFromItemKey(PtB.MetadataEntry), Rule->Tolerance, Rule->ComponentSelection);
		};

		POI->Out->GetMutablePoints().Sort([&Rules, &SortDirection, Compare](const FPCGPoint& A, const FPCGPoint& B)
		{
#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) \
case _ENUM : Result = FPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, Rule.Tolerance, Rule.ComponentSelection); break;

			int Result = 0;
			for (const FPCGExSortRule& Rule : Rules)
			{
				switch (Rule.Selector.GetSelection())
				{
				case EPCGAttributePropertySelection::Attribute:
					Result = PCGMetadataAttribute::CallbackWithRightType(Rule.UnderlyingType, Compare, &Rule, A, B);
					break;
				case EPCGAttributePropertySelection::PointProperty:
					switch (Rule.Selector.GetPointProperty())
					{
					PCGEX_FOREACH_POINTPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (Rule.Selector.GetExtraProperty())
					{
					PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					default: ;
					}
					break;
				default: ;
				}

				if (Result != 0) { break; }
			}

			if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
			return Result <= 0;
		});
	});

	Points.OutputTo(Context);
	
	return true;
}

bool FPCGExSortPointsElement::BuildRulesForPoints(const UPCGPointData* InData, const TArray<FPCGExSortRule>& DesiredRules, TArray<FPCGExSortRule>& OutRules)
{
	OutRules.Empty(DesiredRules.Num());

	for (const FPCGExSortRule& DesiredRule : DesiredRules)
	{
		FPCGExSortRule& Rule = OutRules.Emplace_GetRef(DesiredRule);
		if (!Rule.Validate(InData)) { OutRules.Pop(); }
	}

	return !OutRules.IsEmpty();
}

#undef PCGEX_COMPARE_PROPERTY
#undef PCGEX_COMPARE_PROPERTY_CASE

#undef LOCTEXT_NAMESPACE

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExSortPoints.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPoint.h"
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

#pragma endregion

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const { return MakeShared<FPCGExSortPointsElement>(); }

PCGEx::EIOInit UPCGExSortPointsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsByAttributesElement::Execute);

	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);

	if (!Context->IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidContext", "Inputs are missing or invalid."));
		return true;
	}

	const UPCGExSortPointsSettings* Settings = Context->GetInputSettings<UPCGExSortPointsSettings>();
	check(Settings);

	const TArray<FPCGExSortRule>& DesiredRules = Settings->Rules;

	if (DesiredRules.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSortParams", "No attributes to sort over."));
		return true;
	}

	//

	EPCGExSortDirection SortDirection = Settings->SortDirection;
	TArray<FPCGExSortRule> Rules;

	auto ProcessPair = [&Context, &DesiredRules, &Rules, &SortDirection, this](PCGEx::FPointIO* POI, const int32)
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

		POI->Out->GetMutablePoints().Sort(
			[&Rules, &SortDirection, Compare](
			const FPCGPoint& A, const FPCGPoint& B)
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
	};

	Context->Points.ForEach(Context, ProcessPair);
	Context->Points.OutputTo(Context);

	return true;
}

bool FPCGExSortPointsElement::BuildRulesForPoints(
	const UPCGPointData* InData,
	const TArray<FPCGExSortRule>& DesiredRules,
	TArray<FPCGExSortRule>& OutRules)
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

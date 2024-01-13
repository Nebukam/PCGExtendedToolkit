// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExSortPoints.h"

#include "Misc/PCGExCompare.h"

#define LOCTEXT_NAMESPACE "PCGExSortPoints"
#define PCGEX_NAMESPACE SortPoints

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const { return MakeShared<FPCGExSortPointsElement>(); }

PCGExData::EInit UPCGExSortPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsElement::Execute);

	PCGEX_CONTEXT(PointsProcessor)
	PCGEX_SETTINGS(SortPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		if (Settings->Rules.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No attributes to sort over."));
			return true;
		}

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	auto SortPointIO = [&](const int32 Index)
	{
		PCGExData::FPointIO& PointIO = *Context->MainPoints->Pairs[Index];
		TArray<FPCGExSortRule> Rules;
		Rules.Reserve(Settings->Rules.Num());

		for (const FPCGExSortRule& DesiredRule : Settings->Rules)
		{
			FPCGExSortRule& Rule = Rules.Emplace_GetRef(DesiredRule);
			if (!Rule.Validate(PointIO.GetOut())) { Rules.Pop(); }
		}

		if (Rules.IsEmpty()) { return; } // Could not sort, omit output.

		auto Compare = [&](auto DummyValue, const FPCGExSortRule* Rule, const FPCGPoint& PtA, const FPCGPoint& PtB) -> int
		{
			using T = decltype(DummyValue);
			FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Rule->Attribute);
			return FPCGExCompare::Compare(
				Attribute->GetValueFromItemKey(PtA.MetadataEntry),
				Attribute->GetValueFromItemKey(PtB.MetadataEntry),
				Rule->Tolerance, Rule->OrderFieldSelection);
		};

		auto SortPredicate = [&](const FPCGPoint& A, const FPCGPoint& B)
		{
#define PCGEX_COMPARE_PROPERTY_CASE(_ENUM, _ACCESSOR) \
case _ENUM : Result = FPCGExCompare::Compare(A._ACCESSOR, B._ACCESSOR, Rule.Tolerance, Rule.OrderFieldSelection); break;

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
					}
					break;
				case EPCGAttributePropertySelection::ExtraProperty:
					switch (Rule.Selector.GetExtraProperty())
					{
					PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_COMPARE_PROPERTY_CASE)
					}
					break;
				default: ;
				}

				if (Result != 0) { break; }
			}

			if (Settings->SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
			return Result <= 0;
		};

		PointIO.GetOut()->GetMutablePoints().Sort(SortPredicate);

		{
			FWriteScopeLock WriteLock(Context->ContextLock);
			PointIO.OutputTo(Context);
		}

		Rules.Empty();
	};

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (!Context->Process(SortPointIO, Context->MainPoints->Num())) { return false; }
		Context->Done();
	}

	return Context->IsDone();
}

#undef PCGEX_COMPARE_PROPERTY
#undef PCGEX_COMPARE_PROPERTY_CASE

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

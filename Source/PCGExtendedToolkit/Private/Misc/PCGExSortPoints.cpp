// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExSortPoints.h"

#define LOCTEXT_NAMESPACE "PCGExSortPoints"
#define PCGEX_NAMESPACE SortPoints

#if WITH_EDITOR
void UPCGExSortPointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExSortRuleDescriptor& Descriptor : Rules) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

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
		TArray<FPCGExSortRule*> Rules;
		Rules.Reserve(Settings->Rules.Num());

		TMap<PCGMetadataEntryKey, int32> PointIndices;
		PointIO.PrintInKeysMap(PointIndices);

		for (const FPCGExSortRuleDescriptor& RuleDescriptor : Settings->Rules)
		{
			FPCGExSortRule* NewRule = new FPCGExSortRule();
			NewRule->Capture(RuleDescriptor);
			if (!NewRule->Grab(PointIO))
			{
				delete NewRule;
				continue;
			}
			NewRule->Tolerance = RuleDescriptor.Tolerance;
			Rules.Add(NewRule);
		}

		if (Rules.IsEmpty())
		{
			// Don't sort
			FWriteScopeLock WriteLock(Context->ContextLock);
			PointIO.OutputTo(Context);
			return;
		}

		auto SortPredicate = [&](const FPCGPoint& A, const FPCGPoint& B)
		{
			int Result = 0;
			for (const FPCGExSortRule* Rule : Rules)
			{
				const double ValueA = Rule->Values[*PointIndices.Find(A.MetadataEntry)];
				const double ValueB = Rule->Values[*PointIndices.Find(B.MetadataEntry)];
				Result = FMath::IsNearlyEqual(ValueA, ValueB, Rule->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
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

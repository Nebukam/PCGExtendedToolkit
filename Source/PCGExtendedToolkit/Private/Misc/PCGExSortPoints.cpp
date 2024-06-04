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

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 IOIndex = 0;
		while (Context->AdvancePointsIO()) { Context->GetAsyncManager()->Start<FPCGExSortPointIO>(IOIndex++, Context->CurrentIO); }

		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC

		Context->OutputPoints();
		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExSortPointIO::ExecuteTask()
{
	const FPCGExPointsProcessorContext* Context = Manager->GetContext<FPCGExPointsProcessorContext>();
	PCGEX_SETTINGS(SortPoints);
	TArray<FPCGExSortRule*> Rules;
	Rules.Reserve(Settings->Rules.Num());

	PointIO->CreateOutKeys(); //Initialize metadata keys
	TMap<PCGMetadataEntryKey, int32> PointIndices;
	PointIO->PrintOutKeysMap(PointIndices, true);

	for (const FPCGExSortRuleDescriptor& RuleDescriptor : Settings->Rules)
	{
		FPCGExSortRule* NewRule = new FPCGExSortRule();
		NewRule->Capture(RuleDescriptor);
		if (!NewRule->Grab(*PointIO))
		{
			delete NewRule;
			continue;
		}

		//if (NewRule->bAbsolute) { for (double& Value : NewRule->Values) { Value = FMath::Abs(Value); } }

		NewRule->Tolerance = RuleDescriptor.Tolerance;
		NewRule->bInvertRule = RuleDescriptor.bInvertRule;
		Rules.Add(NewRule);
	}

	if (Rules.IsEmpty())
	{
		// Don't sort
		PointIndices.Empty();
		return false;
	}

	auto SortPredicate = [&](const FPCGPoint& A, const FPCGPoint& B)
	{
		int Result = 0;
		for (const FPCGExSortRule* Rule : Rules)
		{
			const double ValueA = Rule->Values[*PointIndices.Find(A.MetadataEntry)];
			const double ValueB = Rule->Values[*PointIndices.Find(B.MetadataEntry)];
			Result = FMath::IsNearlyEqual(ValueA, ValueB, Rule->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (Rule->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (Settings->SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	};

	PointIO->GetOut()->GetMutablePoints().Sort(SortPredicate);

	PointIndices.Empty();
	PCGEX_DELETE_TARRAY(Rules)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

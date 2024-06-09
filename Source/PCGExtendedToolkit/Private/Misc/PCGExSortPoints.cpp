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

FPCGElementPtr UPCGExSortPointsBaseSettings::CreateElement() const { return MakeShared<FPCGExSortPointsBaseElement>(); }

PCGExData::EInit UPCGExSortPointsBaseSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

bool UPCGExSortPointsBaseSettings::GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleDescriptor>& OutRules) const
{
	return true;
}

bool UPCGExSortPointsSettings::GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleDescriptor>& OutRules) const
{
	if (Rules.IsEmpty()) { return false; }
	for (const FPCGExSortRuleDescriptor& Descriptor : Rules) { OutRules.Add(Descriptor); }
	return true;
}

bool FPCGExSortPointsBaseElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsElement::Execute);

	PCGEX_CONTEXT(PointsProcessor)
	PCGEX_SETTINGS(SortPointsBase)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		TArray<FPCGExSortRuleDescriptor> RuleDescriptors;
		if (!Settings->GetSortingRules(Context, RuleDescriptors))
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

		Context->OutputMainPoints();
		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExSortPointIO::ExecuteTask()
{
	const FPCGExPointsProcessorContext* Context = Manager->GetContext<FPCGExPointsProcessorContext>();
	PCGEX_SETTINGS(SortPointsBase);

	TArray<FPCGExSortRuleDescriptor> RuleDescriptors;
	Settings->GetSortingRules(Context, RuleDescriptors);

	TArray<FPCGExSortRule*> Rules;
	Rules.Reserve(RuleDescriptors.Num());

	PointIO->CreateOutKeys(); //Initialize metadata keys
	TMap<PCGMetadataEntryKey, int32> PointIndices;
	PointIO->PrintOutKeysMap(PointIndices, true);

	for (const FPCGExSortRuleDescriptor& RuleDescriptor : RuleDescriptors)
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

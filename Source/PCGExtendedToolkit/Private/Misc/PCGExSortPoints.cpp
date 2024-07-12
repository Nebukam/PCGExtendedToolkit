// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExSortPoints.h"

#define LOCTEXT_NAMESPACE "PCGExSortPoints"
#define PCGEX_NAMESPACE SortPoints

#if WITH_EDITOR
void UPCGExSortPointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (FPCGExSortRuleConfig& Config : Rules) { Config.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExSortPointsBaseSettings::CreateElement() const { return MakeShared<FPCGExSortPointsBaseElement>(); }

PCGExData::EInit UPCGExSortPointsBaseSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

bool UPCGExSortPointsBaseSettings::GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	return true;
}

bool UPCGExSortPointsSettings::GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	if (Rules.IsEmpty()) { return false; }
	for (const FPCGExSortRuleConfig& Config : Rules) { OutRules.Add(Config); }
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

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (!Settings->GetSortingRules(Context, RuleConfigs))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No attributes to sort over."));
			return true;
		}

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSortPoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSortPoints::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to sort."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExSortPoints
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSortPoints::Process);
		PCGEX_SETTINGS(SortPointsBase)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		Settings->GetSortingRules(Context, RuleConfigs);

		TArray<FPCGExSortRule*> Rules;
		Rules.Reserve(RuleConfigs.Num());

		TMap<PCGMetadataEntryKey, int32> PointIndices;
		PointIO->PrintOutKeysMap(PointIndices, true);

		for (const FPCGExSortRuleConfig& RuleConfig : RuleConfigs)
		{
			FPCGExSortRule* NewRule = new FPCGExSortRule();
			PCGExData::FCache<double>* Cache = PointDataFacade->GetOrCreateGetter<double>(RuleConfig.Selector);

			if (!Cache)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some points are missing attributes used for sorting."));
				delete NewRule;
				continue;
			}

			NewRule->Cache = Cache;
			NewRule->Tolerance = RuleConfig.Tolerance;
			NewRule->bInvertRule = RuleConfig.bInvertRule;
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
				const double ValueA = Rule->Cache->Values[PointIndices[A.MetadataEntry]];
				const double ValueB = Rule->Cache->Values[PointIndices[B.MetadataEntry]];
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

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

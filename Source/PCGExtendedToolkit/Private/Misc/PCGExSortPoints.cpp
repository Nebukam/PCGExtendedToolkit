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
	PCGEX_EXECUTION_CHECK

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
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSortPoints::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to sort."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSortPoints
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSortPoints::Process);
		const UPCGExSortPointsBaseSettings* Settings = ExecutionContext->GetInputSettings<UPCGExSortPointsBaseSettings>();
		check(Settings);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		Settings->GetSortingRules(ExecutionContext, RuleConfigs);

		TArray<TSharedPtr<FPCGExSortRule>> Rules;
		Rules.Reserve(RuleConfigs.Num());

		TMap<PCGMetadataEntryKey, int32> PointIndices;
		PointDataFacade->Source->PrintOutKeysMap(PointIndices);

		for (const FPCGExSortRuleConfig& RuleConfig : RuleConfigs)
		{
			TSharedPtr<FPCGExSortRule> NewRule = MakeShared<FPCGExSortRule>();
			const TSharedPtr<PCGExData::TBuffer<double>> Cache = PointDataFacade->GetBroadcaster<double>(RuleConfig.Selector);

			if (!Cache)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some points are missing attributes used for sorting."));
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
			for (const TSharedPtr<FPCGExSortRule>& Rule : Rules)
			{
				const double ValueA = Rule->Cache->Read(PointIndices[A.MetadataEntry]);
				const double ValueB = Rule->Cache->Read(PointIndices[B.MetadataEntry]);
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

		PointDataFacade->GetOut()->GetMutablePoints().Sort(SortPredicate);

		PointIndices.Empty();
		return true;
	}

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

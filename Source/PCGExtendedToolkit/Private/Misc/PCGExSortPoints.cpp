// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExSortPoints.h"

#include "Misc/PCGExModularSortPoints.h"


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

PCGExData::EIOInit UPCGExSortPointsBaseSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

bool UPCGExSortPointsBaseSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	return true;
}

bool UPCGExSortPointsSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
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
	PCGEX_ON_INITIAL_EXECUTION
	{
		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (!Settings->GetSortingRules(Context, RuleConfigs))
		{
			return Context->CancelExecution(TEXT("No attributes to sort over."));
		}

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSortPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSortPoints::FProcessor>>& NewBatch)
			{
				NewBatch->bPrefetchData = true;
			}))
		{
			Context->CancelExecution(TEXT("Could not find any points to sort."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSortPoints
{
	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TPointsProcessor::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		Settings->GetSortingRules(ExecutionContext, RuleConfigs);

		Sorter = MakeShared<PCGExSorting::PointSorter<true>>(Context, PointDataFacade, RuleConfigs);
		Sorter->SortDirection = Settings->SortDirection;
		Sorter->RegisterBuffersDependencies(FacadePreloader);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSortPoints::Process);

		if (!TPointsProcessor::Process(InAsyncManager)) { return false; }

		if (!Sorter->Init())
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some dataset have no valid sorting rules, they won't be sorted."));
			return false;
		}

		PointDataFacade->GetOut()->GetMutablePoints().Sort([&](const FPCGPoint& A, const FPCGPoint& B) { return Sorter->Sort(A, B); });
		return true;
	}

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

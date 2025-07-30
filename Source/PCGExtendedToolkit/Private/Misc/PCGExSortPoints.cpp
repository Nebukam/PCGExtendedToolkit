// Copyright 2025 Timothé Lapetite and contributors
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

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSortPoints
{
	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		Settings->GetSortingRules(ExecutionContext, RuleConfigs);

		Sorter = MakeShared<PCGExSorting::FPointSorter>(Context, PointDataFacade, RuleConfigs);
		Sorter->SortDirection = Settings->SortDirection;
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSortPoints::Process);

		if (!TProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (!Sorter->Init(Context))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some dataset have no valid sorting rules, they won't be sorted."));
			return false;
		}

		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, PointDataFacade->GetNum());
		Order.Sort([&](const int32 A, const int32 B) { return Sorter->Sort(A, B); });

		PointDataFacade->Source->InheritPoints(Order, 0);

		return true;
	}

	void FProcessor::CompleteWork()
	{
		IProcessor::CompleteWork();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

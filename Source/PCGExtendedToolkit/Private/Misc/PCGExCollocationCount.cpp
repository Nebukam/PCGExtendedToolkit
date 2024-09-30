// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCollocationCount.h"


#define LOCTEXT_NAMESPACE "PCGExCollocationCountElement"
#define PCGEX_NAMESPACE CollocationCount

PCGExData::EInit UPCGExCollocationCountSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(CollocationCount)

bool FPCGExCollocationCountElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CollocationCount)

	PCGEX_VALIDATE_NAME(Settings->CollicationNumAttributeName)
	if (Settings->bWriteLinearOccurences) { PCGEX_VALIDATE_NAME(Settings->LinearOccurencesAttributeName) }

	return true;
}

bool FPCGExCollocationCountElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCollocationCountElement::Execute);

	PCGEX_CONTEXT(CollocationCount)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExCollocationCount::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExCollocationCount::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExCollocationCount
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCollocationCount::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		NumPoints = PointDataFacade->GetNum();
		ToleranceConstant = Settings->Tolerance;

		CollocationWriter = PointDataFacade->GetWritable(Settings->CollicationNumAttributeName, 0, true, true);

		if (Settings->bWriteLinearOccurences)
		{
			LinearOccurencesWriter = PointDataFacade->GetWritable(Settings->LinearOccurencesAttributeName, 0, true, true);
		}

		Octree = &PointDataFacade->Source->GetIn()->GetOctree();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const FVector Center = Point.Transform.GetLocation();
		const double Tolerance = ToleranceConstant;
		FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Center, FVector(Tolerance));

		CollocationWriter->GetMutable(Index) = 0;

		auto ProcessNeighbors = [&](const FPCGPointRef& Other)
		{
			const ptrdiff_t OtherIndex = Other.Point - PointDataFacade->GetIn()->GetPoints().GetData();
			if (OtherIndex == Index) { return; }
			if (FVector::Dist(Center, Other.Point->Transform.GetLocation()) > Tolerance) { return; }

			CollocationWriter->GetMutable(Index) += 1;
		};

		auto ProcessNeighbors2 = [&](const FPCGPointRef& Other)
		{
			const ptrdiff_t OtherIndex = Other.Point - PointDataFacade->GetIn()->GetPoints().GetData();
			if (OtherIndex == Index) { return; }
			if (FVector::Dist(Center, Other.Point->Transform.GetLocation()) > Tolerance) { return; }

			CollocationWriter->GetMutable(Index) += 1;

			if (OtherIndex < Index) { LinearOccurencesWriter->GetMutable(Index) += 1; }
		};

		if (LinearOccurencesWriter)
		{
			LinearOccurencesWriter->GetMutable(Index) = 0;
			Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbors2);
		}
		else
		{
			Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbors);
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

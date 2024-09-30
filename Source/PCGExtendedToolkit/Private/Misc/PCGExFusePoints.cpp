// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Data/Blending/PCGExCompoundBlender.h"


#include "Graph/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExFusePoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFusePoints::Process);


		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		CompoundGraph = MakeUnique<PCGExGraph::FCompoundGraph>(
			Settings->PointPointIntersectionDetails.FuseDetails,
			PointDataFacade->GetIn()->GetBounds().ExpandBy(10));

		const TArray<FPCGPoint>& Points = PointDataFacade->GetIn()->GetPoints();

		bInlineProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		CompoundGraph->InsertPoint(Point, PointDataFacade->Source->IOIndex, Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		PCGExGraph::FCompoundNode* CompoundNode = CompoundGraph->Nodes[Iteration].Get();
		PCGMetadataEntryKey Key = MutablePoints[Iteration].MetadataEntry;
		MutablePoints[Iteration] = CompoundNode->Point; // Copy "original" point properties, in case there's only one

		FPCGPoint& Point = MutablePoints[Iteration];
		Point.MetadataEntry = Key; // Restore key

		Point.Transform.SetLocation(CompoundNode->UpdateCenter(CompoundGraph->PointsCompounds.Get(), Context->MainPoints.Get()));
		CompoundPointsBlender->MergeSingle(Iteration, PCGExDetails::GetDistanceDetails(Settings->PointPointIntersectionDetails));
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumCompoundNodes = CompoundGraph->Nodes.Num();
		PointDataFacade->Source->InitializeNum(NumCompoundNodes);

		CompoundPointsBlender = MakeUnique<PCGExDataBlending::FCompoundBlender>(const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), &Context->CarryOverDetails);
		CompoundPointsBlender->AddSource(PointDataFacade);
		CompoundPointsBlender->PrepareMerge(PointDataFacade, CompoundGraph->PointsCompounds);

		StartParallelLoopForRange(NumCompoundNodes);
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Data/Blending/PCGExUnionBlender.h"


#include "Graph/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EIOInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	Context->Distances = PCGExDetails::MakeDistances(Settings->PointPointIntersectionDetails.FuseDetails.SourceDistance, Settings->PointPointIntersectionDetails.FuseDetails.TargetDistance);

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFusePoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFusePoints::Process);


		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		UnionGraph = MakeShared<PCGExGraph::FUnionGraph>(
			Settings->PointPointIntersectionDetails.FuseDetails,
			PointDataFacade->GetIn()->GetBounds().ExpandBy(10));

		bInlineProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		UnionGraph->InsertPoint(Point, PointDataFacade->Source->IOIndex, Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		const TSharedPtr<PCGExGraph::FUnionNode> UnionNode = UnionGraph->Nodes[Iteration];
		const PCGMetadataEntryKey Key = MutablePoints[Iteration].MetadataEntry;
		MutablePoints[Iteration] = UnionNode->Point; // Copy "original" point properties, in case there's only one

		FPCGPoint& Point = MutablePoints[Iteration];
		Point.MetadataEntry = Key; // Restore key

		Point.Transform.SetLocation(UnionNode->UpdateCenter(UnionGraph->NodesUnion, Context->MainPoints));
		UnionBlender->MergeSingle(Iteration, Context->Distances);
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumUnionNodes = UnionGraph->Nodes.Num();
		PointDataFacade->Source->GetOut()->GetMutablePoints().SetNum(NumUnionNodes);

		UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), &Context->CarryOverDetails);
		UnionBlender->AddSource(PointDataFacade, &PCGExGraph::ProtectedClusterAttributes);
		UnionBlender->PrepareMerge(Context, PointDataFacade, UnionGraph->NodesUnion);

		StartParallelLoopForRange(NumUnionNodes);
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

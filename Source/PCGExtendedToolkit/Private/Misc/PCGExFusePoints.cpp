// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Data/Blending/PCGExCompoundBlender.h"
#include "Graph/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGContext* InContext) const
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExFusePoints
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(CompoundGraph)
		PCGEX_DELETE(CompoundPointsBlender)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFusePoints::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FusePoints)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointIO->CreateInKeys();

		CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionDetails.FuseDetails,
			PointIO->GetIn()->GetBounds().ExpandBy(10),
			Settings->PointPointIntersectionDetails.FuseMethod);

		const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();

		bInlineProcessPoints = Settings->PointPointIntersectionDetails.DoInlineInsertion();
		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		CompoundGraph->InsertPoint(Point, PointIO->IOIndex, Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		PCGExGraph::FCompoundNode* CompoundNode = CompoundGraph->Nodes[Iteration];
		PCGMetadataEntryKey Key = MutablePoints[Iteration].MetadataEntry;
		MutablePoints[Iteration] = CompoundNode->Point; // Copy "original" point properties, in case there's only one

		FPCGPoint& Point = MutablePoints[Iteration];
		Point.MetadataEntry = Key; // Restore key

		Point.Transform.SetLocation(CompoundNode->UpdateCenter(CompoundGraph->PointsCompounds, LocalTypedContext->MainPoints));
		CompoundPointsBlender->MergeSingle(Iteration, PCGExDetails::GetDistanceDetails(LocalSettings->PointPointIntersectionDetails));
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FusePoints)

		const int32 NumCompoundNodes = CompoundGraph->Nodes.Num();
		PointIO->InitializeNum(NumCompoundNodes);

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), &TypedContext->CarryOverDetails);
		CompoundPointsBlender->AddSource(PointDataFacade);
		CompoundPointsBlender->PrepareMerge(PointDataFacade, CompoundGraph->PointsCompounds);

		StartParallelLoopForRange(NumCompoundNodes);
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

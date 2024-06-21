// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "MeshUtilitiesCommon.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

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

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FusePoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointIO->CreateInKeys();

		CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionSettings.FuseSettings,
			PointIO->GetIn()->GetBounds().ExpandBy(10), true,
			Settings->PointPointIntersectionSettings.Precision == EPCGExFusePrecision::Fast);
		AsyncManagerPtr->Start<PCGExGraphTask::FCompoundGraphInsertPoints>(PointIO->IOIndex, PointIO, CompoundGraph);

		if (CompoundGraph->bFastMode) { StartParallelLoopForPoints(PCGExData::ESource::In); }
		else
		{
			const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
			for (int i = 0; i < Points.Num(); i++) { CompoundGraph->GetOrCreateNode(Points[i], PointIO->IOIndex, i); }
		}

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
	{
		CompoundGraph->GetOrCreateNode(Point, PointIO->IOIndex, Index);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FusePoints)

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		PCGExGraph::FCompoundNode* CompoundNode = CompoundGraph->Nodes[Iteration];
		PCGMetadataEntryKey Key = MutablePoints[Iteration].MetadataEntry;
		MutablePoints[Iteration] = CompoundNode->Point; // Copy "original" point properties, in case there's only one

		FPCGPoint& Point = MutablePoints[Iteration];
		Point.MetadataEntry = Key; // Restore key

		Point.Transform.SetLocation(CompoundNode->UpdateCenter(CompoundGraph->PointsCompounds, TypedContext->MainPoints));
		CompoundPointsBlender->MergeSingle(Iteration, PCGExSettings::GetDistanceSettings(Settings->PointPointIntersectionSettings));
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FusePoints)

		const int32 NumCompoundNodes = CompoundGraph->Nodes.Num();
		PointIO->SetNumInitialized(NumCompoundNodes);

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
		CompoundPointsBlender->AddSources(*TypedContext->MainPoints);
		CompoundPointsBlender->PrepareMerge(PointIO, CompoundGraph->PointsCompounds);

		StartParallelLoopForRange(NumCompoundNodes);
	}

	void FProcessor::Write()
	{
		CompoundPointsBlender->Write(AsyncManagerPtr);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

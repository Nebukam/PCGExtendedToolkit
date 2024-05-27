// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Data/Blending/PCGExCompoundBlender.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundGraph)
}

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	Context->GraphMetadataSettings.Grab(Context, Settings->PointPointIntersectionSettings);

	PCGEX_FWD(bPreserveOrder)

	return true;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->CompoundGraph)
		PCGEX_DELETE(Context->CompoundPointsBlender)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
			Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

			// Fuse points into compound graph

			Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Settings->PointPointIntersectionSettings.FuseSettings, Context->CurrentIO->GetIn()->GetBounds().ExpandBy(10));
			Context->GetAsyncManager()->Start<PCGExGraphTask::FCompoundGraphInsertPoints>(
				Context->CurrentIO->IOIndex, Context->CurrentIO, Context->CompoundGraph);

			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC

		// Build output points from compound graph
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();

		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { MutablePoints.SetNumUninitialized(NumCompoundNodes); };

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index] = CompoundNode->Point;
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging
		Context->CompoundPointsBlender->PrepareMerge(Context->CurrentIO, Context->CompoundGraph->PointsCompounds);
		Context->SetState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		auto MergeCompound = [&](const int32 CompoundIndex)
		{
			Context->CompoundPointsBlender->MergeSingle(CompoundIndex, PCGExSettings::GetDistanceSettings(Settings->PointPointIntersectionSettings));
		};

		if (!Context->Process(MergeCompound, Context->CompoundGraph->NumNodes())) { return false; }

		Context->CompoundPointsBlender->Write();

		/*
		// Blend attributes & properties
		Context->GetAsyncManager()->Start<PCGExDataBlendingTask::FBlendCompoundedIO>(
			Context->CurrentIO->IOIndex, Context->CurrentIO, Context->CurrentIO, const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings),
			Context->CompoundGraph->PointsCompounds, PCGExSettings::GetDistanceSettings(*ISettings), &Context->GraphMetadataSettings);
*/

		Context->CurrentIO->Flatten();

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
		return false;
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

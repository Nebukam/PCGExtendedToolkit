// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Graph/PCGExGraph.h"

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

	PCGEX_FWD(PointPointIntersectionSettings)

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointIntersectionSettings);

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

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			// Fuse points into compound graph
			Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Context->PointPointIntersectionSettings.FuseSettings, Context->CurrentIO->GetIn()->GetBounds().ExpandBy(10));
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
		if (MutablePoints.Num() != NumCompoundNodes) { MutablePoints.SetNum(NumCompoundNodes); }

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(ProcessNode, NumCompoundNodes)) { return false; }

		Context->SetAsyncState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		PCGEX_WAIT_ASYNC

		const FPCGExPointPointIntersectionSettings* ISettings = const_cast<FPCGExPointPointIntersectionSettings*>(&Settings->PointPointIntersectionSettings);

		// Blend attributes & properties
		Context->GetAsyncManager()->Start<PCGExDataBlendingTask::FBlendCompoundedIO>(
			Context->CurrentIO->IOIndex, Context->CurrentIO, Context->CurrentIO, const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings),
			Context->CompoundGraph->PointsCompounds, PCGExSettings::GetDistanceSettings(*ISettings), &Context->GraphMetadataSettings);

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

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

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExUnpackClusters.h"


#include "Clusters/PCGExClustersHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExUnpackClusters"
#define PCGEX_NAMESPACE UnpackClusters

TArray<FPCGPinProperties> UPCGExUnpackClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExClusters::Labels::SourcePackedClustersLabel, "Packed clusters", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUnpackClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExClusters::Labels::OutputEdgesLabel, "Edges associated with the main output points", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(UnpackClusters)

bool FPCGExUnpackClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)

	Context->OutPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutPoints->OutputPin = PCGExClusters::Labels::OutputVerticesLabel;

	Context->OutEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutEdges->OutputPin = PCGExClusters::Labels::OutputEdgesLabel;

	return true;
}


class FPCGExUnpackClusterTask final : public PCGExMT::FTask
{
public:
	PCGEX_ASYNC_TASK_NAME(FPCGExUnpackClusterTask)

	explicit FPCGExUnpackClusterTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
		: FTask(), PointIO(InPointIO)
	{
	}

	TSharedPtr<PCGExData::FPointIO> PointIO;

	virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
	{
		const FPCGExUnpackClustersContext* Context = TaskManager->GetContext<FPCGExUnpackClustersContext>();
		PCGEX_SETTINGS(UnpackClusters)

		FPCGAttributeIdentifier EdgeCountIdentifier = PCGExMetaHelpers::GetAttributeIdentifier(PCGExClusters::Labels::Tag_PackedClusterEdgeCount, PointIO->GetIn());
		const FPCGMetadataAttribute<int32>* EdgeCount = PCGExMetaHelpers::TryGetConstAttribute<int32>(PointIO->GetIn(), EdgeCountIdentifier);
		int32 NumEdges = -1;

		if (!EdgeCount)
		{
			// Support for legacy data that was storing the edge count as a point index
			EdgeCountIdentifier = PCGExMetaHelpers::GetAttributeIdentifier(PCGExClusters::Labels::Tag_PackedClusterEdgeCount_LEGACY, PointIO->GetIn());
			EdgeCount = PCGExMetaHelpers::TryGetConstAttribute<int32>(PointIO->GetIn(), EdgeCountIdentifier);
			if (EdgeCount) { NumEdges = PCGExData::Helpers::ReadDataValue(EdgeCount); }
		}
		else
		{
			NumEdges = PCGExData::Helpers::ReadDataValue(EdgeCount);
		}

		if (NumEdges == -1)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have no packing metadata."));
			return;
		}

		const int32 NumVtx = PointIO->GetNum() - NumEdges;

		if (NumEdges > PointIO->GetNum() || NumVtx <= 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have could not be unpacked correctly (wrong number of vtx or edges)."));
			return;
		}

		const UPCGBasePointData* PackedPoints = PointIO->GetIn();
		EPCGPointNativeProperties AllocateProperties = PackedPoints->GetAllocatedProperties();

		const TSharedPtr<PCGExData::FPointIO> NewEdges = Context->OutEdges->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
		UPCGBasePointData* MutableEdgePoints = NewEdges->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(MutableEdgePoints, NumEdges, AllocateProperties);
		NewEdges->InheritPoints(0, 0, NumEdges);

		NewEdges->DeleteAttribute(EdgeCountIdentifier);
		NewEdges->DeleteAttribute(PCGExClusters::Labels::Attr_PCGExVtxIdx);

		const TSharedPtr<PCGExData::FPointIO> NewVtx = Context->OutPoints->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
		UPCGBasePointData* MutableVtxPoints = NewVtx->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(MutableVtxPoints, NumVtx, AllocateProperties);
		NewVtx->InheritPoints(NumEdges, 0, NumVtx);

		NewVtx->DeleteAttribute(EdgeCountIdentifier);
		NewVtx->DeleteAttribute(PCGExClusters::Labels::Attr_PCGExEdgeIdx);

		const PCGExDataId PairId = PCGEX_GET_DATAIDTAG(PointIO->Tags, PCGExClusters::Labels::TagStr_PCGExCluster);

		PCGExClusters::Helpers::MarkClusterVtx(NewVtx, PairId);
		PCGExClusters::Helpers::MarkClusterEdges(NewEdges, PairId);
	}
};

bool FPCGExUnpackClustersElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUnpackClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
		while (Context->AdvancePointsIO(false)) { PCGEX_LAUNCH(FPCGExUnpackClusterTask, Context->CurrentIO) }
		Context->SetState(PCGExCommon::States::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_WaitingOnAsyncWork)
	{
		Context->OutPoints->StageOutputs();
		Context->OutEdges->StageOutputs();

		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

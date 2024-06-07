// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMeshToClusters.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoMesh.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE MeshToClusters

namespace PCGExGeoTask
{
	class FLloydRelax3;
}

PCGExData::EInit UPCGExMeshToClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExMeshToClustersContext::~FPCGExMeshToClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(StaticMeshMap)
	PCGEX_DELETE_TARRAY(GraphBuilders)

	MeshIdx.Empty();
}

TArray<FPCGPinProperties> UPCGExMeshToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExMeshToClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

bool UPCGExMeshToClustersSettings::GetMainAcceptMultipleData() const { return false; }

PCGEX_INITIALIZE_ELEMENT(MeshToClusters)

FName UPCGExMeshToClustersSettings::GetMainInputLabel() const { return PCGEx::SourceTargetsLabel; }

bool FPCGExMeshToClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)

	if (Context->MainPoints->Pairs.Num() < 1)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing targets."));
		return false;
	}

	PCGExData::FPointIO* Targets = Context->MainPoints->Pairs[0];
	Context->MeshIdx.SetNum(Targets->GetNum());
	Context->GraphBuilders.SetNum(Targets->GetNum());

	Context->StaticMeshMap = new PCGExGeo::FGeoStaticMeshMap();

	if (Settings->StaticMeshSource == EPCGExFetchType::Constant)
	{
		if (!Settings->StaticMeshConstant.ToSoftObjectPath().IsValid())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid static mesh constant"));
			return false;
		}

		const int32 Idx = Context->StaticMeshMap->Find(Settings->StaticMeshConstant.ToSoftObjectPath());

		if (Idx == -1)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Static mesh constant could not be loaded."));
			return false;
		}

		for (int32& Index : Context->MeshIdx) { Index = Idx; }
	}
	else
	{
		PCGEX_VALIDATE_NAME(Settings->StaticMeshAttribute)

		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.SetAttributeName(Settings->StaticMeshAttribute);

		PCGEx::FLocalToStringGetter* PathGetter = new PCGEx::FLocalToStringGetter();
		PathGetter->Capture(Selector);
		if (!PathGetter->SoftGrab(*Context->MainPoints->Pairs[0]))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Static mesh attribute does not exists on targets."));
			return false;
		}

		const TArray<FPCGPoint>& TargetPoints = Targets->GetIn()->GetPoints();
		for (int i = 0; i < TargetPoints.Num(); i++)
		{
			Context->GraphBuilders[i] = nullptr;

			FSoftObjectPath Path = PathGetter->SoftGet(TargetPoints[i], TEXT(""));

			if (!Path.IsValid())
			{
				if (!Settings->bIgnoreMeshWarnings) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Some targets could not have their mesh loaded.")); }
				Context->MeshIdx[i] = -1;
				continue;
			}

			const int32 Idx = Context->StaticMeshMap->Find(Path);

			if (Idx == -1)
			{
				if (!Settings->bIgnoreMeshWarnings) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Some targets could not have their mesh loaded.")); }
				Context->MeshIdx[i] = -1;
			}
			else
			{
				Context->MeshIdx[i] = Idx;
			}
		}
	}

	return true;
}

bool FPCGExMeshToClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMeshToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			for (PCGExGeo::FGeoStaticMesh* GSM : Context->StaticMeshMap->GSMs)
			{
				const FStaticMeshLODResources& LODResources = GSM->StaticMesh->GetRenderData()->LODResources[0];
				GSM->ExtractMeshAsync(Context->GetAsyncManager());
			} // Preload all
			Context->SetAsyncState(PCGExGeo::State_ExtractingMesh);
		}
	}

	if (Context->IsState(PCGExGeo::State_ExtractingMesh))
	{
		PCGEX_WAIT_ASYNC
		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessTarget = [&](const int32 TargetIndex, const PCGExData::FPointIO& PointIO)
		{
			if (Context->MeshIdx[TargetIndex] == -1) { return; }
			Context->GetAsyncManager()->Start<FPCGExMeshToClusterTask>(TargetIndex, Context->CurrentIO);
		};

		if (!Context->ProcessCurrentPoints(ProcessTarget)) { return false; }

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		Context->OutputPoints();

		for (int i = 0; i < Context->GraphBuilders.Num(); i++)
		{
			const PCGExGraph::FGraphBuilder* Builder = Context->GraphBuilders[i];
			
			if (!Builder || !Builder->bCompiledSuccessfully) { continue; }
			Builder->Write(Context);
		}

		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExMeshToClusterTask::ExecuteTask()
{
	FPCGExMeshToClustersContext* Context = static_cast<FPCGExMeshToClustersContext*>(Manager->Context);
	PCGEX_SETTINGS(MeshToClusters)

	const FPCGPoint& PointTarget = PointIO->GetInPoint(TaskIndex);
	const PCGExGeo::FGeoStaticMesh* StaticMesh = Context->StaticMeshMap->GetMesh(Context->MeshIdx[TaskIndex]);

	PCGExData::FPointIO& VtxIO = Context->MainPoints->Emplace_GetRef();

	VtxIO.SetNumInitialized(StaticMesh->Vertices.Num());

	TArray<FPCGPoint>& VtxPoints = VtxIO.GetOut()->GetMutablePoints();

	for (int i = 0; i < VtxPoints.Num(); i++)
	{
		FPCGPoint& NewVtx = VtxPoints[i];
		NewVtx.Transform.SetLocation(PointTarget.Transform.TransformPosition(StaticMesh->Vertices[i]));
	}

	PCGExGraph::FGraphBuilder* GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &Context->BuilderSettings, 6);
	GraphBuilder->Graph->InsertEdges(StaticMesh->Edges, -1);

	Context->GraphBuilders[TaskIndex] = GraphBuilder;
	GraphBuilder->Compile(Context);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

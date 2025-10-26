﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopologyPointSurface.h"

#include "PCGComponent.h"
#include "Data/PCGDynamicMeshData.h" // Redundant but required for build on Linux 
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE TopologyPointSurface

namespace PCGExGeoTask
{
	class FLloydRelax2;
}

TArray<FPCGPinProperties> UPCGExTopologyPointSurfaceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_MESH(PCGExTopology::OutputMeshLabel, "PCG Dynamic Mesh", Normal)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(TopologyPointSurface)

void FPCGExTopologyPointSurfaceContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(TopologyPointSurface)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	if (Settings->Topology.Material.ToSoftObjectPath().IsValid())
	{
		AddAssetDependency(Settings->Topology.Material.ToSoftObjectPath());
	}
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(TopologyPointSurface)

bool FPCGExTopologyPointSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TopologyPointSurface)

	return true;
}

bool FPCGExTopologyPointSurfaceElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTopologyPointSurfaceElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TopologyPointSurface)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 3 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bHasInvalidInputs = true;
					return false;
				}

				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainBatch->Output();

	return Context->TryComplete();
}

namespace PCGExTopologyPointSurface
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExTopologyPointSurface::Process);

		PointDataFacade->bSupportsScopedGet = false;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		// Prep data

		bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

		InternalMeshData = Context->ManagedObjects->New<UPCGDynamicMeshData>();
		if (!InternalMeshData) { return false; }

		InternalMesh = Context->ManagedObjects->New<UDynamicMesh>();
		InternalMesh->InitializeMesh();

		if (InternalMeshData)
		{
			InternalMeshData->Initialize(InternalMesh, true);
			InternalMesh = InternalMeshData->GetMutableDynamicMesh();
			if (UMaterialInterface* Material = Settings->Topology.Material.Get()) { InternalMeshData->SetMaterials({Material}); }
		}

		// Project points

		ProjectionDetails = Settings->ProjectionDetails;
		if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { if (!ProjectionDetails.Init(PointDataFacade)) { return false; } }
		else { ProjectionDetails.Init(PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

		// Build delaunay

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->Source->GetIn()->GetConstTransformValueRange();

		TArray<FVector2D> VertexPositions;
		VertexPositions.Init(FVector2D::ZeroVector, InTransforms.Num());
		ProjectionDetails.Project(InTransforms, VertexPositions);

		TArray<FIntPoint> ConstrainedEdges;
		TArray<int32> PositionsToVertexIDs;
		bool bHasDuplicateVertices = false;

		FGeometryScriptConstrainedDelaunayTriangulationOptions TriangulationOptions{};
		TriangulationOptions.bRemoveDuplicateVertices = true;

		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDelaunayTriangulation2D(
			InternalMesh, Settings->Topology.PrimitiveOptions, FTransform::Identity,
			VertexPositions, ConstrainedEdges, TriangulationOptions, PositionsToVertexIDs,
			bHasDuplicateVertices, nullptr);

		if (PositionsToVertexIDs.IsEmpty()) { return false; }

		FTransform Transform = Context->GetComponent()->GetOwner()->GetTransform();
		Transform.SetScale3D(FVector::OneVector);
		Transform.SetRotation(FQuat::Identity);

		InternalMesh->EditMesh(
			[&](FDynamicMesh3& InMesh)
			{
				const int32 VtxCount = InMesh.MaxVertexID();
				const TConstPCGValueRange<FVector4> InColors = PointDataFacade->GetIn()->GetConstColorValueRange();

				InMesh.EnableAttributes();
				InMesh.Attributes()->EnablePrimaryColors();
				InMesh.Attributes()->EnableMaterialID();

				UE::Geometry::FDynamicMeshColorOverlay* Colors = InMesh.Attributes()->PrimaryColors();
				UE::Geometry::FDynamicMeshMaterialAttribute* MaterialID = InMesh.Attributes()->GetMaterialID();

				TArray<int32> ElemIDs;
				ElemIDs.SetNum(VtxCount);

				for (int i = 0; i < VtxCount; i++)
				{
					const int32 VtxID = PositionsToVertexIDs[i];
					InMesh.SetVertex(VtxID, Transform.InverseTransformPosition(InTransforms[i].GetLocation()));
					ElemIDs[i] = Colors->AppendElement(FVector4f(InColors[i]));
				}

				for (int32 TriangleID : InMesh.TriangleIndicesItr())
				{
					UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
					MaterialID->SetValue(TriangleID, 0);
					Colors->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
				}
			}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);


		if (Settings->bAttemptRepair)
		{
			UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(
				InternalMesh, Settings->RepairDegenerate);
		}

		Settings->Topology.PostProcessMesh(InternalMesh);

		return true;
	}

	void FProcessor::Output()
	{
		if (!this->bIsProcessorValid) { return; }

		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

		if (InternalMeshData)
		{
			TSet<FString> MeshTags;
			Context->StageOutput(InternalMeshData, PCGExTopology::MeshOutputLabel, MeshTags, true, false, false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

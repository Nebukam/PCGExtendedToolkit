// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Triangulate.h"

#include "PCGComponent.h"
#include "UDynamicMesh.h"
#include "Data/PCGDynamicMeshData.h"
#include "Data/PCGExPointIO.h"
#include "Clipper2Lib/clipper.h"
#include "Clipper2Lib/clipper.triangulation.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "GeometryScript/MeshRepairFunctions.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2TriangulateElement"
#define PCGEX_NAMESPACE Clipper2Triangulate

TArray<FPCGPinProperties> UPCGExClipper2TriangulateSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_MESH(PCGExTopology::MeshOutputLabel, "PCG Dynamic Mesh", Normal)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(Clipper2Triangulate)

FPCGExGeo2DProjectionDetails UPCGExClipper2TriangulateSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

uint64 FPCGExClipper2TriangulateContext::HashPoint(int64 X, int64 Y)
{
	return PCGEx::H64(
		static_cast<uint32>(X & 0xFFFFFFFF),
		static_cast<uint32>(Y & 0xFFFFFFFF));
}

int32 FPCGExClipper2TriangulateContext::FindVertexIndex(int64 X, int64 Y) const
{
	const uint64 Hash = HashPoint(X, Y);
	const int32* Found = VertexMap.Find(Hash);
	return Found ? *Found : -1;
}

void FPCGExClipper2TriangulateContext::BuildVertexPoolFromGroup(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2TriangulateSettings* Settings = GetInputSettings<UPCGExClipper2TriangulateSettings>();
	const double InvScale = 1.0 / static_cast<double>(Settings->Precision);

	// Process all subject paths
	for (const int32 SubjectIdx : Group->SubjectIndices)
	{
		if (SubjectIdx >= AllOpData->Paths.Num()) { continue; }

		const auto& Path = AllOpData->Paths[SubjectIdx];
		const auto& Facade = AllOpData->Facades[SubjectIdx];
		const auto& Projection = AllOpData->Projections[SubjectIdx];

		TConstPCGValueRange<FTransform> Transforms = Facade->Source->GetIn()->GetConstTransformValueRange();
		TConstPCGValueRange<FVector4> Colors = Facade->Source->GetIn()->GetConstColorValueRange();

		for (size_t i = 0; i < Path.size(); ++i)
		{
			const auto& Pt = Path[i];
			const uint64 Hash = HashPoint(Pt.x, Pt.y);

			// Skip if already in pool
			if (VertexMap.Contains(Hash)) { continue; }

			// Decode source info from Z
			uint32 PointIdx, SourceIdx;
			PCGEx::H64(static_cast<uint64>(Pt.z), PointIdx, SourceIdx);

			FPCGExTriangulationVertex Vertex;
			Vertex.ClipperX = Pt.x;
			Vertex.ClipperY = Pt.y;
			Vertex.SourceDataIndex = static_cast<int32>(SourceIdx);
			Vertex.SourcePointIndex = static_cast<int32>(PointIdx);

			// Get position from source or unproject
			if (PointIdx < static_cast<uint32>(Transforms.Num()))
			{
				Vertex.Position = Transforms[PointIdx].GetLocation();
				Vertex.Color = Colors[PointIdx];
			}
			else
			{
				// Unproject from Clipper2 coordinates
				Vertex.Position = Projection.Unproject(
					FVector(
						static_cast<double>(Pt.x) * InvScale,
						static_cast<double>(Pt.y) * InvScale,
						0.0),
					0);
				Vertex.Color = FVector4(1, 1, 1, 1);
			}

			const int32 PoolIndex = VertexPool.Num();
			VertexMap.Add(Hash, PoolIndex);
			VertexPool.Add(MoveTemp(Vertex));
		}
	}
}

void FPCGExClipper2TriangulateContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2TriangulateSettings* Settings = GetInputSettings<UPCGExClipper2TriangulateSettings>();

	if (!Group->IsValid()) { return; }
	if (Group->SubjectPaths.empty()) { return; }

	// Build vertex pool from this group's paths
	BuildVertexPoolFromGroup(Group);

	// Accumulate tags
	if (!OutputTags) { OutputTags = MakeShared<PCGExData::FTags>(); }
	OutputTags->Append(Group->GroupTags.ToSharedRef());

	// Combine subject paths for triangulation
	PCGExClipper2Lib::Paths64 CombinedPaths;
	CombinedPaths.reserve(Group->SubjectPaths.size());
	for (const auto& Path : Group->SubjectPaths)
	{
		CombinedPaths.push_back(Path);
	}

	// Perform triangulation
	PCGExClipper2Lib::Paths64 TrianglePaths;
	PCGExClipper2Lib::TriangulateResult Result =
		PCGExClipper2Lib::TriangulateWithHoles(
			CombinedPaths,
			TrianglePaths,
			PCGExClipper2::ConvertFillRule(Settings->FillRule), // or NonZero
			Settings->bUseDelaunay                              // useDelaunay
		);

	if (Result != PCGExClipper2Lib::TriangulateResult::success)
	{
		switch (Result)
		{
		case PCGExClipper2Lib::TriangulateResult::fail:
			PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Triangulation failed."));
			break;
		case PCGExClipper2Lib::TriangulateResult::no_polygons:
			PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("No valid polygons for triangulation."));
			break;
		case PCGExClipper2Lib::TriangulateResult::paths_intersect:
			PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Paths intersect - triangulation requires non-intersecting paths. Consider using Boolean Union first."));
			break;
		default:
			break;
		}
		return;
	}

	// Convert triangle paths to indexed triangles
	for (const auto& TriPath : TrianglePaths)
	{
		if (TriPath.size() != 3) { continue; }

		const int32 V0 = FindVertexIndex(TriPath[0].x, TriPath[0].y);
		const int32 V1 = FindVertexIndex(TriPath[1].x, TriPath[1].y);
		const int32 V2 = FindVertexIndex(TriPath[2].x, TriPath[2].y);

		// Skip triangles with unknown vertices
		if (V0 < 0 || V1 < 0 || V2 < 0)
		{
			if (!Settings->bQuietBadVerticesWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Triangle references unknown vertex - skipping."));
			}
			continue;
		}

		// Skip degenerate triangles
		if (V0 == V1 || V1 == V2 || V2 == V0) { continue; }

		Triangles.Add(FIntVector(V0, V1, V2));
	}
}

void FPCGExClipper2TriangulateContext::BuildMesh()
{
	const UPCGExClipper2TriangulateSettings* Settings = GetInputSettings<UPCGExClipper2TriangulateSettings>();

	if (VertexPool.IsEmpty() || Triangles.IsEmpty()) { return; }

	// Create mesh objects
	MeshData = ManagedObjects->New<UPCGDynamicMeshData>();
	if (!MeshData) { return; }

	Mesh = ManagedObjects->New<UDynamicMesh>();
	Mesh->InitializeMesh();

	MeshData->Initialize(Mesh, true);
	Mesh = MeshData->GetMutableDynamicMesh();

	if (UMaterialInterface* Material = Settings->Topology.Material.Get())
	{
		MeshData->SetMaterials({Material});
	}

	// Get component transform for inverse transform
	FTransform Transform = FTransform::Identity;
	if (const UPCGComponent* Component = GetComponent())
	{
		Transform = Component->GetOwner()->GetTransform();
		Transform.SetScale3D(FVector::OneVector);
		Transform.SetRotation(FQuat::Identity);
	}

	const int32 NumVertices = VertexPool.Num();
	const int32 NumTriangles = Triangles.Num();
	const int32 MaxSourceIndex = AllOpData->Facades.Num();

	// Build source tracking arrays for UV writing
	TArray<int32> SourceDataIndices;
	TArray<int32> SourcePointIndices;
	SourceDataIndices.SetNum(NumVertices);
	SourcePointIndices.SetNum(NumVertices);

	for (int32 i = 0; i < NumVertices; ++i)
	{
		SourceDataIndices[i] = VertexPool[i].SourceDataIndex;
		SourcePointIndices[i] = VertexPool[i].SourcePointIndex;
	}

	// Arrays to collect for UV processing
	TArray<int32> VertexIDs;
	TArray<int32> ColorElemIDs;
	TArray<int32> TriangleIDs;

	VertexIDs.SetNum(NumVertices);
	ColorElemIDs.SetNum(NumVertices);
	TriangleIDs.Reserve(NumTriangles);

	Mesh->EditMesh([&](FDynamicMesh3& InMesh)
	{
		// Enable attributes
		InMesh.EnableAttributes();
		InMesh.Attributes()->EnablePrimaryColors();
		InMesh.Attributes()->EnableMaterialID();

		UE::Geometry::FDynamicMeshColorOverlay* Colors = InMesh.Attributes()->PrimaryColors();
		UE::Geometry::FDynamicMeshMaterialAttribute* MaterialID = InMesh.Attributes()->GetMaterialID();

		// Add vertices
		for (int32 i = 0; i < NumVertices; i++)
		{
			const FPCGExTriangulationVertex& Vtx = VertexPool[i];

			// Add vertex position (transformed to local space)
			VertexIDs[i] = InMesh.AppendVertex(Transform.InverseTransformPosition(Vtx.Position));

			// Prepare color
			ColorElemIDs[i] = Colors->AppendElement(FVector4f(Vtx.Color));
		}

		// Add triangles
		for (int32 i = 0; i < NumTriangles; i++)
		{
			const FIntVector& Tri = Triangles[i];

			const int32 TriID = InMesh.AppendTriangle(
				VertexIDs[Tri.X],
				VertexIDs[Tri.Y],
				VertexIDs[Tri.Z]);

			if (TriID >= 0)
			{
				MaterialID->SetValue(TriID, 0);
				Colors->SetTriangle(
					TriID,
					UE::Geometry::FIndex3i(
						ColorElemIDs[Tri.X],
						ColorElemIDs[Tri.Y],
						ColorElemIDs[Tri.Z]));
				TriangleIDs.Add(TriID);
			}
		}

		// Write UVs using multi-facade lookup
		Settings->Topology.UVChannels.Write(
			TriangleIDs,
			VertexIDs,
			SourceDataIndices,
			SourcePointIndices,
			AllOpData->Facades,
			InMesh);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

	// Attempt repair if requested
	if (Settings->bAttemptRepair)
	{
		UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(Mesh, Settings->RepairDegenerate);
	}

	// Post-process mesh
	Settings->Topology.PostProcessMesh(Mesh);
}

bool FPCGExClipper2TriangulateElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClipper2ProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Triangulate)

	// Reserve space for vertex pool
	int32 TotalPoints = 0;
	for (const auto& Facade : Context->AllOpData->Facades)
	{
		TotalPoints += Facade->Source->GetNum();
	}
	Context->VertexPool.Reserve(TotalPoints);
	Context->VertexMap.Reserve(TotalPoints);
	Context->Triangles.Reserve(TotalPoints * 2); // Rough estimate

	return true;
}

void FPCGExClipper2TriangulateElement::OutputWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Triangulate)

	Context->BuildMesh();

	// Output the mesh
	if (Context->MeshData)
	{
		TSet<FString> Tags;
		if (Context->OutputTags) { Tags = Context->OutputTags->Flatten(); }

		Context->StageOutput(
			Context->MeshData,
			PCGExTopology::MeshOutputLabel,
			PCGExData::EStaging::Managed,
			Tags);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

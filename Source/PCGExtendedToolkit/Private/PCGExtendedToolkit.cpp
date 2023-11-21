// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkit.h"

#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Transforms/PCGExSampleNearestSurface.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitModule"

UStaticMesh* FPCGExtendedToolkitModule::DebugMeshFrustrum = nullptr;
void FPCGExtendedToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	BuildDebugStaticMeshes();
	
}

void FPCGExtendedToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FPCGExtendedToolkitModule::BuildFrustrum(FMeshDescription& MeshDescription)
{
	int32 NumVertices = 5;
	int32 NumTriangles = 4;

	// Define the vertices of the pyramid
	FVector3f VBL(-0.5, -0.5, 1);
	FVector3f VBR(0.5, -0.5, 1);
	FVector3f VTL(0.5, 0.5, 1);
	FVector3f VTR(-0.5, 0.5, 1);
	FVector3f VApex(0, 0, 0);

	FPolygonGroupID GroupID = MeshDescription.CreatePolygonGroup();

	// Add vertices to the mesh builder
	FVertexInstanceID BL = MeshDescription.CreateVertexInstance(MeshDescription.CreateVertex());
	FVertexInstanceID BR = MeshDescription.CreateVertexInstance(MeshDescription.CreateVertex());
	FVertexInstanceID TL = MeshDescription.CreateVertexInstance(MeshDescription.CreateVertex());
	FVertexInstanceID TR = MeshDescription.CreateVertexInstance(MeshDescription.CreateVertex());
	FVertexInstanceID Apex = MeshDescription.CreateVertexInstance(MeshDescription.CreateVertex());

	MeshDescription.CreateTriangle(GroupID, {BL, BR, Apex});
	MeshDescription.CreateTriangle(GroupID, {BR, TL, Apex});
	MeshDescription.CreateTriangle(GroupID, {TL, TR, Apex});
	MeshDescription.CreateTriangle(GroupID, {TR, BL, Apex});

	//fill vertex data
	auto positions = MeshDescription.GetVertexPositions();
	//auto uvs = meshDesc.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate).GetRawArray();
	//auto normals = meshDesc.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal).GetRawArray();
	positions[0] = VBL;
	positions[1] = VBR;
	positions[2] = VTL;
	positions[3] = VTR;
	positions[4] = VApex;
}

void FPCGExtendedToolkitModule::BuildDebugStaticMeshes()
{
	//TODO: This doesn't seem to work.
	DebugMeshFrustrum = CreateVirtualStaticMesh(FName("PCGExFrustrum"), [this](FMeshDescription& MD) { BuildFrustrum(MD); });
}

template <typename BuilderFunc>
UStaticMesh* FPCGExtendedToolkitModule::CreateVirtualStaticMesh(FName MeshName, BuilderFunc&& BuildDescription)
{
	FMeshDescription MeshDescription;
	FStaticMeshAttributes attributes(MeshDescription);
	attributes.Register();

	BuildDescription(MeshDescription);

	// Create a new StaticMesh
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), MeshName, RF_Transient);
	StaticMesh->SetRenderData(MakeUnique<FStaticMeshRenderData>());
	//StaticMesh->CreateBodySetup();
	//StaticMesh->bAllowCPUAccess = true;
	//StaticMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	const UStaticMesh::FBuildMeshDescriptionsParams MDParams;

	// Notify the asset registry about the created object
	FAssetRegistryModule::AssetCreated(StaticMesh);

	StaticMesh->BuildFromMeshDescriptions({&MeshDescription}, MDParams);
	return StaticMesh;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitModule, PCGExtendedToolkit)

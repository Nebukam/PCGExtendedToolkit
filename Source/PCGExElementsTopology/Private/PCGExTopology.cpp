// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExTopology.h"

#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"

#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Paths/PCGExPath.h"

void FPCGExTopologyDetails::PostProcessMesh(const TObjectPtr<UDynamicMesh>& InDynamicMesh) const
{
	if (bWeldEdges) { UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(InDynamicMesh, WeldEdgesOptions); }
	if (bComputeNormals) { UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(InDynamicMesh, NormalsOptions); }
}

void FPCGExTopologyUVDetails::Prepare(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	TSet<int32> ExistingChannels;
	ExistingChannels.Reserve(8);
	ChannelIndices.Reserve(8);
	UVBuffers.Reserve(8);

	for (const FPCGExUVInputDetails& Channel : UVs)
	{
		if (!Channel.bEnabled || Channel.AttributeName.IsNone()) { continue; }

		bool bIsAlreadySet = false;
		ExistingChannels.Add(Channel.Channel, &bIsAlreadySet);
		if (bIsAlreadySet) { continue; }

		TSharedPtr<PCGExData::TBuffer<FVector2D>> Buffer = InDataFacade->GetBroadcaster<FVector2D>(Channel.AttributeName, true);
		if (!Buffer)
		{
			PCGEX_LOG_INVALID_ATTR_C(InDataFacade->GetContext(), UV Channel, Channel.AttributeName)
			continue;
		}

		NumChannels = FMath::Max(NumChannels, Channel.Channel + 1);
		ChannelIndices.Add(Channel.Channel);
		UVBuffers.Add(Buffer);
	}
}

void FPCGExTopologyUVDetails::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	TSet<int32> ExistingChannels;
	ExistingChannels.Reserve(8);

	for (const FPCGExUVInputDetails& Channel : UVs)
	{
		if (!Channel.bEnabled || Channel.AttributeName.IsNone()) { continue; }

		bool bIsAlreadySet = false;
		ExistingChannels.Add(Channel.Channel, &bIsAlreadySet);
		if (bIsAlreadySet) { continue; }

		FacadePreloader.Register<FVector2D>(InContext, Channel.AttributeName, PCGExData::EBufferPreloadType::BroadcastFromName);
	}
}

void FPCGExTopologyUVDetails::Write(const TArray<int32>& TriangleIDs, UE::Geometry::FDynamicMesh3& InMesh) const
{
	if (!NumChannels) { return; }

	const int32 VtxCount = InMesh.MaxVertexID();
	InMesh.Attributes()->SetNumUVLayers(NumChannels);

	TArray<int32> ElemIDs;
	ElemIDs.SetNum(VtxCount);

	for (int t = 0; t < ChannelIndices.Num(); ++t)
	{
		const int32 ChannelIndex = ChannelIndices[t];
		const TSharedPtr<PCGExData::TBuffer<FVector2D>> UVBuffer = UVBuffers[t];

		UE::Geometry::FDynamicMeshUVOverlay* UV = InMesh.Attributes()->GetUVLayer(ChannelIndex);

		for (int i = 0; i < VtxCount; i++) { ElemIDs[i] = UV->AppendElement(FVector2f(UVBuffer->Read(i))); }

		for (const int32 TriangleID : TriangleIDs)
		{
			const UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
			UV->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
		}
	}
}

void FPCGExTopologyUVDetails::Write(const TArray<int32>& TriangleIDs, const TArray<int32>& VtxIDs, UE::Geometry::FDynamicMesh3& InMesh) const
{
	if (!NumChannels) { return; }

	const int32 VtxCount = InMesh.MaxVertexID();
	InMesh.Attributes()->SetNumUVLayers(NumChannels);

	TArray<int32> ElemIDs;
	ElemIDs.SetNum(VtxCount);


	for (int t = 0; t < ChannelIndices.Num(); ++t)
	{
		const int32 ChannelIndex = ChannelIndices[t];
		const TSharedPtr<PCGExData::TBuffer<FVector2D>> UVBuffer = UVBuffers[t];
		UE::Geometry::FDynamicMeshUVOverlay* UV = InMesh.Attributes()->GetUVLayer(ChannelIndex);

		for (int i = 0; i < VtxCount; i++)
		{
			const int32 PtIdx = VtxIDs[i];
			ElemIDs[i] = UV->AppendElement(FVector2f(PtIdx != -1 ? UVBuffer->Read(PtIdx) : FVector2D(0)));
		}

		for (const int32 TriangleID : TriangleIDs)
		{
			const UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
			UV->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
		}
	}
}

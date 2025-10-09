﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeoMesh.h"

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExH.h"
#include "PCGExHelpers.h"
#include "PCGParamData.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"
#include "Async/ParallelFor.h"
#include "Data/PCGExAttributeMapHelpers.h"

bool FPCGExGeoMeshImportDetails::Validate(FPCGExContext* InContext)
{
	if (bImportUVs)
	{
		PCGEx::BuildMap(InContext, PCGExGeo::SourceUVImportRulesLabel, UVChannels);

		if (UVChannels.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Import UV channel is true, but there is no import details."));
			return true;
		}

		TSet<FName> UniqueNames;
		UVChannelIndex.Reserve(UVChannels.Num());
		for (const TPair<FName, int32>& Pair : UVChannels)
		{
			if (Pair.Value < 0)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel mapping has an illegal channel index (< 0) and will be ignored."));
				continue;
			}

			if (Pair.Value > 7)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel mapping has an illegal channel index (> 7) and will be ignored."));
				continue;
			}

			bool bNameAlreadyExists = false;
			UniqueNames.Add(Pair.Key, &bNameAlreadyExists);
			if (bNameAlreadyExists)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel name is used more than once. Only the first entry will be used."));
				continue;
			}

			if (!PCGEx::IsWritableAttributeName(Pair.Key))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("A channel name is not a valid attribute name, it will be ignored."));
				continue;
			}

			UVChannelId.Add(FPCGAttributeIdentifier(Pair.Key, PCGMetadataDomainID::Elements));
			UVChannelIndex.Add(Pair.Value);
		}
	}

	return true;
}

bool FPCGExGeoMeshImportDetails::WantsImport() const
{
	return bImportVertexColor || !UVChannels.IsEmpty();
}

namespace PCGExGeo
{
	void DeclareGeoMeshImportInputs(const FPCGExGeoMeshImportDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
	{
		if (!InDetails.bImportUVs) { return; }

		{
			PCGEX_PIN_PARAMS(SourceUVImportRulesLabel, "Name/Channel output map. Attribute sets are expected to contain an FName attribute an int32 attribute.", Normal)
		}
	}

	FMeshLookup::FMeshLookup(const int32 Size, TArray<FVector>* InVertices, TArray<int32>* InRawIndices, const FVector& InHashTolerance)
		: Vertices(InVertices), RawIndices(InRawIndices), HashTolerance(InHashTolerance)
	{
		Data.Reserve(Size);
		Vertices->Reserve(Size);
		if (RawIndices) { RawIndices->Reserve(Size); }
	}

	uint32 FMeshLookup::Add_GetIdx(const FVector& Position, const int32 RawIndex)
	{
		const uint32 Key = PCGEx::GH3(Position, HashTolerance);
		if (const int32* IdxPtr = Data.Find(Key)) { return *IdxPtr; }

		const int32 Idx = Vertices->Emplace(Position);
		Data.Add(Key, Idx);

		if (RawIndices) { RawIndices->Emplace(RawIndex); }

		return Idx;
	}

	void FGeoMesh::MakeDual()
	// Need triangulate first
	{
		if (Triangles.IsEmpty()) { return; }

		TArray<FVector> DualPositions;
		PCGEx::InitArray(DualPositions, Triangles.Num());

		Edges.Empty();

		for (int i = 0; i < Triangles.Num(); i++)
		{
			FIntVector3& Triangle = Triangles[i];
			DualPositions[i] = (Vertices[Triangle.X] + Vertices[Triangle.Y] + Vertices[Triangle.Z]) / 3;

			Triangle.X = RawIndices[Triangle.X];
			Triangle.Y = RawIndices[Triangle.Y];
			Triangle.Z = RawIndices[Triangle.Z];

			const FIntVector3& Adjacency = Tri_Adjacency[i];
			if (Adjacency.X != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.X)); }
			if (Adjacency.Y != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.Y)); }
			if (Adjacency.Z != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.Z)); }
		}

		// Raw indices have been mutated and stored in triangles instead.
		RawIndices.SetNum(Triangles.Num());
		for (int i = 0; i < Triangles.Num(); i++) { RawIndices[i] = -(i + 1); }

		Vertices.Reset(DualPositions.Num());
		Vertices = MoveTemp(DualPositions);

		Tri_Adjacency.Empty();
	}

	void FGeoMesh::MakeHollowDual()
	// Need triangulate first
	{
		if (Triangles.IsEmpty()) { return; }

		const int32 StartIndex = Vertices.Num();
		const int32 NumTriangles = Triangles.Num();
		Vertices.SetNum(StartIndex + NumTriangles);
		RawIndices.SetNum(StartIndex + NumTriangles);

		Edges.Empty();

		for (int i = 0; i < NumTriangles; i++)
		{
			const FIntVector3& Triangle = Triangles[i];
			const int32 E = StartIndex + i;

			Vertices[E] = (Vertices[Triangle.X] + Vertices[Triangle.Y] + Vertices[Triangle.Z]) / 3;
			RawIndices[E] = -(i + 1);

			Edges.Add(PCGEx::H64U(E, Triangle.X));
			Edges.Add(PCGEx::H64U(E, Triangle.Y));
			Edges.Add(PCGEx::H64U(E, Triangle.Z));
		}

		Tri_Adjacency.Empty();
	}

	FGeoStaticMesh::FGeoStaticMesh(const TSoftObjectPtr<UStaticMesh>& InSoftStaticMesh)
	{
		if (!InSoftStaticMesh.ToSoftObjectPath().IsValid()) { return; }

		StaticMesh = PCGExHelpers::LoadBlocking_AnyThread(InSoftStaticMesh);
		if (!StaticMesh) { return; }

		StaticMesh->GetRenderData();
		bIsValid = true;
	}

	FGeoStaticMesh::FGeoStaticMesh(const FSoftObjectPath& InSoftStaticMesh)
		: FGeoStaticMesh(TSoftObjectPtr<UStaticMesh>(InSoftStaticMesh))
	{
	}

	FGeoStaticMesh::FGeoStaticMesh(const FString& InSoftStaticMesh):
		FGeoStaticMesh(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(InSoftStaticMesh)))
	{
	}

	void FGeoStaticMesh::ExtractMeshSynchronous()
	{
		if (bIsLoaded) { return; }
		if (!bIsValid) { return; }

		LODResource = &StaticMesh->GetRenderData()->LODResources[0];

		if (!LODResource)
		{
			bIsValid = false;
			return;
		}

		const FStaticMeshVertexBuffers& VertexBuffers = LODResource->VertexBuffers;

		bHasColorData = VertexBuffers.ColorVertexBuffer.IsInitialized() && VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0;
		const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;

		const FIndexArrayView& Indices = LODResource->IndexBuffer.GetArrayView();
		const int32 NumTriangles = Indices.Num() / 3;

		TUniquePtr<FMeshLookup> MeshLookup = MakeUnique<FMeshLookup>(PositionBuffer.GetNumVertices() / 3, &Vertices, &RawIndices, CWTolerance);
		Edges.Reserve(NumTriangles / 2);

		for (int i = 0; i < Indices.Num(); i += 3)
		{
			const int32 RawA = Indices[i];
			const int32 RawB = Indices[i + 1];
			const int32 RawC = Indices[i + 2];

			const uint32 A = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawA)), RawA);
			const uint32 B = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawB)), RawB);
			const uint32 C = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawC)), RawC);

			if (A != B) { Edges.Add(PCGEx::H64U(A, B)); }
			if (B != C) { Edges.Add(PCGEx::H64U(B, C)); }
			if (C != A) { Edges.Add(PCGEx::H64U(C, A)); }
		}

		bIsLoaded = true;
	}

	void FGeoStaticMesh::TriangulateMeshSynchronous()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGeoStaticMesh::TriangulateMeshSynchronous);

		if (bIsLoaded) { return; }
		if (!bIsValid) { return; }

		LODResource = &StaticMesh->GetRenderData()->LODResources[0];

		if (!LODResource)
		{
			bIsValid = false;
			return;
		}

		const FStaticMeshVertexBuffers& VertexBuffers = LODResource->VertexBuffers;

		bHasColorData = VertexBuffers.ColorVertexBuffer.IsInitialized() && VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0;
		const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;

		Edges.Empty();

		TUniquePtr<FMeshLookup> MeshLookup = MakeUnique<FMeshLookup>(PositionBuffer.GetNumVertices() / 3, &Vertices, &RawIndices, CWTolerance);

		const FIndexArrayView& Indices = LODResource->IndexBuffer.GetArrayView();

		const int32 NumTriangles = Indices.Num() / 3;
		Triangles.Init(FIntVector3(-1), NumTriangles);
		Tri_Adjacency.Init(FIntVector3(-1), NumTriangles);
		
		TBitArray<> Tri_IsOnHull;
		Tri_IsOnHull.Init(true, NumTriangles);

		TMap<uint64, int32> EdgeMap;
		EdgeMap.Reserve(NumTriangles / 2);

		auto PushAdjacency = [&](const int32 Tri, const int32 OtherTri)
		{
			FIntVector3& Adjacency = Tri_Adjacency[Tri];
			for (int i = 0; i < 3; i++)
			{
				if (Adjacency[i] == -1)
				{
					Adjacency[i] = OtherTri;
					if (i == 2) { Tri_IsOnHull[Tri] = false; }
					break;
				}
			}
		};

		auto PushEdge = [&](const int32 Tri, const uint64 Edge)
		{
			bool bIsAlreadySet = false;
			Edges.Add(Edge, &bIsAlreadySet);
			if (bIsAlreadySet)
			{
				if (int32 OtherTri = -1;
					EdgeMap.RemoveAndCopyValue(Edge, OtherTri))
				{
					PushAdjacency(OtherTri, Tri);
					PushAdjacency(Tri, OtherTri);
				}
			}
			else
			{
				EdgeMap.Add(Edge, Tri);
			}
		};

		int32 Ti = 0;
		for (int i = 0; i < Indices.Num(); i += 3)
		{
			const int32 RawA = Indices[i];
			const int32 RawB = Indices[i + 1];
			const int32 RawC = Indices[i + 2];

			const uint32 A = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawA)), RawA);
			const uint32 B = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawB)), RawB);
			const uint32 C = MeshLookup->Add_GetIdx(FVector(PositionBuffer.VertexPosition(RawC)), RawC);

			if (A == B || B == C || C == A) { continue; }

			Triangles[Ti] = FIntVector3(A, B, C);

			PushEdge(Ti, PCGEx::H64U(A, B));
			PushEdge(Ti, PCGEx::H64U(B, C));
			PushEdge(Ti, PCGEx::H64U(A, C));

			Ti++;
		}

		Triangles.SetNum(Ti);
		if (Triangles.IsEmpty())
		{
			bIsValid = false;
			return;
		}

		for (int i = 0; i < Triangles.Num(); i++)
		{
			FIntVector3& Tri = Triangles[i];
			const int32 A = Tri[0];
			const int32 B = Tri[1];
			const int32 C = Tri[2];

			if (Tri_IsOnHull[i])
			{
				const uint64 AB = PCGEx::H64U(A, B);
				const uint64 BC = PCGEx::H64U(B, C);
				const uint64 AC = PCGEx::H64U(A, C);

				// Push edges that are still waiting to be matched
				if (EdgeMap.Contains(AB))
				{
					HullIndices.Add(A);
					HullIndices.Add(B);
					HullEdges.Add(AB);
				}

				if (EdgeMap.Contains(BC))
				{
					HullIndices.Add(B);
					HullIndices.Add(C);
					HullEdges.Add(BC);
				}

				if (EdgeMap.Contains(AC))
				{
					HullIndices.Add(A);
					HullIndices.Add(C);
					HullEdges.Add(AC);
				}
			}
		}

		bIsLoaded = true;
	}

	void FGeoStaticMesh::ExtractMeshAsync(PCGExMT::FTaskManager* AsyncManager)
	{
		if (bIsLoaded) { return; }
		if (!bIsValid) { return; }

		PCGEX_LAUNCH(FExtractStaticMeshTask, SharedThis(this))
	}

	int32 FGeoStaticMeshMap::Find(const FSoftObjectPath& InPath)
	{
		if (const int32* GSMPtr = Map.Find(InPath)) { return *GSMPtr; }

		PCGEX_MAKE_SHARED(GSM, FGeoStaticMesh, InPath)
		if (!GSM->bIsValid) { return -1; }

		const int32 Index = GSMs.Add(GSM);
		GSM->DesiredTriangulationType = DesiredTriangulationType;
		Map.Add(InPath, Index);
		return Index;
	}

	FExtractStaticMeshTask::FExtractStaticMeshTask(const TSharedPtr<FGeoStaticMesh>& InGSM) :
		FTask(), GSM(InGSM)
	{
	}

	void FExtractStaticMeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		GSM->ExtractMeshSynchronous();
	}
}

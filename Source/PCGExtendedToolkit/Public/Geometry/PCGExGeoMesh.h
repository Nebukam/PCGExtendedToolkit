// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "PCGExHelpers.h"

//#include "PCGExGeoMesh.generated.h"

UENUM()
enum class EPCGExTriangulationType : uint8
{
	Raw    = 0 UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual   = 1 UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow = 2 UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
};

namespace PCGExGeo
{
	class FMeshLookup : public TSharedFromThis<FMeshLookup>
	{
	protected:
		uint32 InternalIdx = 0;

	public:
		TMap<uint32, TTuple<int32, uint32>> Data;

		explicit FMeshLookup(const int32 Size)
		{
			Data.Reserve(Size);
		}

		FORCEINLINE uint32 Add_GetIdx(const uint32 Key, const int32 Value)
		{
			if (TTuple<int32, uint32>* Tuple = Data.Find(Key)) { return Tuple->Get<1>(); }
			const int32 SnapIdx = InternalIdx;
			Data.Add(Key, TTuple<int32, uint32>(Value, InternalIdx++));
			return SnapIdx;
		}

		FORCEINLINE int32 Num() const { return Data.Num(); }
	};

	class FExtractStaticMeshTask;

	class /*PCGEXTENDEDTOOLKIT_API*/ FGeoMesh : public TSharedFromThis<FGeoMesh>
	{
	public:
		bool bIsValid = false;
		bool bIsLoaded = false;
		TArray<FVector> Vertices;
		TSet<uint64> Edges;
		TArray<FIntVector3> Triangles;
		TArray<FIntVector3> Adjacencies;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoMesh()
		{
		}

		void MakeDual() // Need triangulate first
		{
			if (Triangles.IsEmpty()) { return; }

			TArray<FVector> DualPositions;
			PCGEx::InitArray(DualPositions, Triangles.Num());

			Edges.Empty();

			for (int i = 0; i < Triangles.Num(); i++)
			{
				const FIntVector3& Triangle = Triangles[i];
				DualPositions[i] = (Vertices[Triangle.X] + Vertices[Triangle.Y] + Vertices[Triangle.Z]) / 3;

				const FIntVector3& Adjacency = Adjacencies[i];
				if (Adjacency.X != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.X)); }
				if (Adjacency.Y != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.Y)); }
				if (Adjacency.Z != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.Z)); }
			}

			Vertices.Empty(DualPositions.Num());
			Vertices.Append(DualPositions);
			DualPositions.Empty();

			Triangles.Empty();
			Adjacencies.Empty();
		}

		void MakeHollowDual() // Need triangulate first
		{
			if (Triangles.IsEmpty()) { return; }

			const int32 StartIndex = Vertices.Num();
			TArray<FVector> DualPositions;
			PCGEx::InitArray(DualPositions, Triangles.Num());
			PCGEx::InitArray(Vertices, StartIndex + Triangles.Num());

			Edges.Empty();

			for (int i = 0; i < Triangles.Num(); i++)
			{
				const FIntVector3& Triangle = Triangles[i];
				const int32 E = StartIndex + i;
				Vertices[E] = (Vertices[Triangle.X] + Vertices[Triangle.Y] + Vertices[Triangle.Z]) / 3;

				Edges.Add(PCGEx::H64U(E, Triangle.X));
				Edges.Add(PCGEx::H64U(E, Triangle.Y));
				Edges.Add(PCGEx::H64U(E, Triangle.Z));
			}

			Triangles.Empty();
			Adjacencies.Empty();
		}

		~FGeoMesh() = default;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGeoStaticMesh : public FGeoMesh
	{
	public:
		TObjectPtr<UStaticMesh> StaticMesh;
		FVector CWTolerance = FVector(1 / 0.001);

		explicit FGeoStaticMesh(const TSoftObjectPtr<UStaticMesh>& InSoftStaticMesh)
		{
			if (!InSoftStaticMesh.ToSoftObjectPath().IsValid()) { return; }

			StaticMesh = InSoftStaticMesh.LoadSynchronous();
			if (!StaticMesh) { return; }

			StaticMesh->GetRenderData();
			bIsValid = true;
		}

		explicit FGeoStaticMesh(const FSoftObjectPath& InSoftStaticMesh):
			FGeoStaticMesh(TSoftObjectPtr<UStaticMesh>(InSoftStaticMesh))
		{
		}

		explicit FGeoStaticMesh(const FString& InSoftStaticMesh):
			FGeoStaticMesh(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(InSoftStaticMesh)))
		{
		}

		void ExtractMeshSynchronous()
		{
			if (bIsLoaded) { return; }
			if (!bIsValid) { return; }

			const FStaticMeshLODResources& LODResources = StaticMesh->GetRenderData()->LODResources[0];

			const FPositionVertexBuffer& VertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;

			TUniquePtr<FMeshLookup> MeshLookup = MakeUnique<FMeshLookup>(VertexBuffer.GetNumVertices() / 3);
			Edges.Empty();

			int32 Idx = 0;
			const FIndexArrayView& Indices = LODResources.IndexBuffer.GetArrayView();
			for (int i = 0; i < Indices.Num(); i += 3)
			{
				const int32 AIdx = Indices[i];
				const int32 BIdx = Indices[i + 1];
				const int32 CIdx = Indices[i + 2];

				const uint32 A = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(AIdx), CWTolerance), AIdx);
				const uint32 B = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(BIdx), CWTolerance), BIdx);
				const uint32 C = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(CIdx), CWTolerance), CIdx);

				Edges.Add(PCGEx::H64U(A, B));
				Edges.Add(PCGEx::H64U(B, C));
				Edges.Add(PCGEx::H64U(C, A));
			}

			PCGEx::InitArray(Vertices, MeshLookup->Num());
			for (const TPair<uint32, TTuple<int32, uint32>>& Pair : MeshLookup->Data) { Vertices[Pair.Value.Get<1>()] = FVector(VertexBuffer.VertexPosition(Pair.Value.Get<0>())); }

			bIsLoaded = true;
		}

		void TriangulateMeshSynchronous()
		{
			if (bIsLoaded) { return; }
			if (!bIsValid) { return; }

			Edges.Empty();

			const FStaticMeshLODResources& LODResources = StaticMesh->GetRenderData()->LODResources[0];

			const FPositionVertexBuffer& VertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;

			TUniquePtr<FMeshLookup> MeshLookup = MakeUnique<FMeshLookup>(VertexBuffer.GetNumVertices() / 3);
			TMap<uint64, uint64> EdgeAdjacency;

			int32 Idx = 0;
			const FIndexArrayView& Indices = LODResources.IndexBuffer.GetArrayView();

			PCGEx::InitArray(Triangles, Indices.Num() / 3);
			int32 TriangleIndex = 0;

			for (int i = 0; i < Indices.Num(); i += 3)
			{
				const int32 AIdx = Indices[i];
				const int32 BIdx = Indices[i + 1];
				const int32 CIdx = Indices[i + 2];

				const uint32 A = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(AIdx), CWTolerance), AIdx);
				const uint32 B = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(BIdx), CWTolerance), BIdx);
				const uint32 C = MeshLookup->Add_GetIdx(PCGEx::GH3(VertexBuffer.VertexPosition(CIdx), CWTolerance), CIdx);

				Edges.Add(PCGEx::H64U(A, B));
				Edges.Add(PCGEx::H64U(B, C));
				Edges.Add(PCGEx::H64U(C, A));

				const uint64 AB = PCGEx::H64U(A, B);
				const uint64 BC = PCGEx::H64U(B, C);
				const uint64 AC = PCGEx::H64U(A, C);

				Edges.Add(AB);
				Edges.Add(BC);
				Edges.Add(AC);

				// Triangles and adjacency

				// Each edge can store its adjacency as a uint64 : left triangle index, right triangle index

				const uint64* AdjacencyABPtr = EdgeAdjacency.Find(AB);
				const uint64* AdjacencyBCPtr = EdgeAdjacency.Find(BC);
				const uint64* AdjacencyACPtr = EdgeAdjacency.Find(AC);

				Triangles[TriangleIndex] = FIntVector3(A, B, C);

				if (AdjacencyABPtr) { EdgeAdjacency.Add(AB, PCGEx::NH64(TriangleIndex, PCGEx::NH64B(*AdjacencyABPtr))); }
				else { EdgeAdjacency.Add(AB, PCGEx::NH64(-1, TriangleIndex)); }

				if (AdjacencyBCPtr) { EdgeAdjacency.Add(BC, PCGEx::NH64(TriangleIndex, PCGEx::NH64B(*AdjacencyBCPtr))); }
				else { EdgeAdjacency.Add(BC, PCGEx::NH64(-1, TriangleIndex)); }

				if (AdjacencyACPtr) { EdgeAdjacency.Add(AC, PCGEx::NH64(TriangleIndex, PCGEx::NH64B(*AdjacencyACPtr))); }
				else { EdgeAdjacency.Add(AC, PCGEx::NH64(-1, TriangleIndex)); }

				TriangleIndex++;
			}

			int32 ENum = EdgeAdjacency.Num();
			PCGEx::InitArray(Adjacencies, Triangles.Num());

			for (int j = 0; j < Triangles.Num(); j++)
			{
				FIntVector3 Triangle = Triangles[j];

				uint64* AdjacencyABPtr = EdgeAdjacency.Find(PCGEx::H64U(Triangle.X, Triangle.Y));
				uint64* AdjacencyBCPtr = EdgeAdjacency.Find(PCGEx::H64U(Triangle.Y, Triangle.Z));
				uint64* AdjacencyACPtr = EdgeAdjacency.Find(PCGEx::H64U(Triangle.X, Triangle.Z));

				Adjacencies[j] = FIntVector3(
					AdjacencyABPtr ? PCGEx::NH64NOT(*AdjacencyABPtr, j) : -1,
					AdjacencyBCPtr ? PCGEx::NH64NOT(*AdjacencyBCPtr, j) : -1,
					AdjacencyACPtr ? PCGEx::NH64NOT(*AdjacencyACPtr, j) : -1);
			}

			EdgeAdjacency.Empty();

			PCGEx::InitArray(Vertices, MeshLookup->Num());
			for (const TPair<uint32, TTuple<int32, uint32>>& Pair : MeshLookup->Data) { Vertices[Pair.Value.Get<1>()] = FVector(VertexBuffer.VertexPosition(Pair.Value.Get<0>())); }

			bIsLoaded = true;
		}

		void ExtractMeshAsync(PCGExMT::FTaskManager* AsyncManager)
		{
			if (bIsLoaded) { return; }
			if (!bIsValid) { return; }
			AsyncManager->Start<FExtractStaticMeshTask>(-1, nullptr, SharedThis(this));
		}

		~FGeoStaticMesh()
		{
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGeoStaticMeshMap : public FGeoMesh
	{
	public:
		TMap<FSoftObjectPath, int32> Map;
		TArray<TSharedPtr<FGeoStaticMesh>> GSMs;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoStaticMeshMap()
		{
		}

		int32 Find(const FSoftObjectPath& InPath)
		{
			if (const int32* GSMPtr = Map.Find(InPath)) { return *GSMPtr; }

			const TSharedPtr<FGeoStaticMesh> GSM = MakeShared<FGeoStaticMesh>(InPath);
			if (!GSM->bIsValid) { return -1; }

			const int32 Index = GSMs.Add(GSM);
			GSM->DesiredTriangulationType = DesiredTriangulationType;
			Map.Add(InPath, Index);
			return Index;
		}

		TSharedPtr<FGeoStaticMesh> GetMesh(const int32 Index) { return GSMs[Index]; }

		~FGeoStaticMeshMap()
		{
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FExtractStaticMeshTask final : public PCGExMT::FPCGExTask
	{
	public:
		FExtractStaticMeshTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TSharedPtr<FGeoStaticMesh>& InGSM) :
			FPCGExTask(InPointIO), GSM(InGSM)
		{
		}

		TSharedPtr<FGeoStaticMesh> GSM;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			GSM->ExtractMeshSynchronous();
			return true;
		}
	};
}

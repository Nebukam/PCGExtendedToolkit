// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
//#include "PCGExGeoMesh.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Graph Triangulation Type"))
enum class EPCGExTriangulationType : uint8
{
	Raw UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
};

namespace PCGExGeo
{
	constexpr PCGExMT::AsyncState State_ExtractingMesh = __COUNTER__;

	class FExtractStaticMeshTask;

	class PCGEXTENDEDTOOLKIT_API FGeoMesh
	{
	public:
		bool bIsValid = false;
		bool bIsLoaded = false;
		TArray<FVector> Vertices;
		TSet<uint64> Edges;
		TArray<UE::Geometry::FIndex3i> Triangles;
		TArray<UE::Geometry::FIndex3i> Adjacencies;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoMesh()
		{
		}

		void MakeDual() // Need triangulate first
		{
			if (Triangles.Num()) { return; }

			TArray<FVector> DualPositions;
			DualPositions.SetNumUninitialized(Triangles.Num());

			Edges.Empty();

			for (int i = 0; i < Triangles.Num(); i++)
			{
				const UE::Geometry::FIndex3i& Triangle = Triangles[i];
				DualPositions[i] = (Vertices[Triangle.A] + Vertices[Triangle.B] + Vertices[Triangle.C]) / 3;

				const UE::Geometry::FIndex3i& Adjacency = Adjacencies[i];
				if (Adjacency.A != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.A)); }
				if (Adjacency.B != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.B)); }
				if (Adjacency.C != -1) { Edges.Add(PCGEx::H64U(i, Adjacency.C)); }
			}

			Vertices.Empty(DualPositions.Num());
			Vertices.Append(DualPositions);
			DualPositions.Empty();

			Triangles.Empty();
			Adjacencies.Empty();
		}

		void MakeHollowDual() // Need triangulate first
		{
			if (Triangles.Num()) { return; }

			const int32 StartIndex = Vertices.Num();
			TArray<FVector> DualPositions;
			DualPositions.SetNumUninitialized(Triangles.Num());
			Vertices.SetNumUninitialized(StartIndex + Triangles.Num());

			Edges.Empty();

			for (int i = 0; i < Triangles.Num(); i++)
			{
				const UE::Geometry::FIndex3i& Triangle = Triangles[i];
				const int32 E = StartIndex + i;
				Vertices[E] = (Vertices[Triangle.A] + Vertices[Triangle.B] + Vertices[Triangle.C]) / 3;

				Edges.Add(PCGEx::H64U(E, Triangle.A));
				Edges.Add(PCGEx::H64U(E, Triangle.B));
				Edges.Add(PCGEx::H64U(E, Triangle.C));
			}

			Triangles.Empty();
			Adjacencies.Empty();
		}

		~FGeoMesh()
		{
			Vertices.Empty();
			Edges.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FGeoStaticMesh : public FGeoMesh
	{
	public:
		TObjectPtr<UStaticMesh> StaticMesh;

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

			TMap<FVector, int32> IndexedUniquePositions;
			Edges.Empty();

			//for (int i = 0; i < GSM->Vertices.Num(); i++) { GSM->Vertices[i] = FVector(VertexBuffer.VertexPosition(i)); }

			int32 Idx = 0;
			const FIndexArrayView& Indices = LODResources.IndexBuffer.GetArrayView();
			for (int i = 0; i < Indices.Num(); i += 3)
			{
				const FVector VA = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i])));
				const FVector VB = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i + 1])));
				const FVector VC = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i + 2])));

				const int32* APtr = IndexedUniquePositions.Find(VA);
				const int32* BPtr = IndexedUniquePositions.Find(VB);
				const int32* CPtr = IndexedUniquePositions.Find(VC);

				const uint32 A = APtr ? *APtr : IndexedUniquePositions.FindOrAdd(VA, Idx++);
				const uint32 B = BPtr ? *BPtr : IndexedUniquePositions.FindOrAdd(VB, Idx++);
				const uint32 C = CPtr ? *CPtr : IndexedUniquePositions.FindOrAdd(VC, Idx++);

				Edges.Add(PCGEx::H64U(A, B));
				Edges.Add(PCGEx::H64U(B, C));
				Edges.Add(PCGEx::H64U(C, A));
			}

			Vertices.SetNum(IndexedUniquePositions.Num());

			TArray<FVector> Keys;
			IndexedUniquePositions.GetKeys(Keys);
			for (FVector Key : Keys) { Vertices[*IndexedUniquePositions.Find(Key)] = Key; }

			IndexedUniquePositions.Empty();
			
			bIsLoaded = true;
		}

		void TriangulateMeshSynchronous()
		{
			if (bIsLoaded) { return; }
			if (!bIsValid) { return; }

			const FStaticMeshLODResources& LODResources = StaticMesh->GetRenderData()->LODResources[0];

			const FPositionVertexBuffer& VertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;

			TMap<FVector, int32> IndexedUniquePositions;
			Edges.Empty();
			TMap<uint64, uint64> EdgeSides;

			int32 Idx = 0;
			const FIndexArrayView& Indices = LODResources.IndexBuffer.GetArrayView();

			Triangles.Reset(Indices.Num() / 3);
			EdgeSides.Reserve(Triangles.Num());

			for (int i = 0; i < Indices.Num(); i += 3)
			{
				const FVector VA = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i])));
				const FVector VB = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i + 1])));
				const FVector VC = PCGExMath::Round10(FVector(VertexBuffer.VertexPosition(Indices[i + 2])));

				const int32* APtr = IndexedUniquePositions.Find(VA);
				const int32* BPtr = IndexedUniquePositions.Find(VB);
				const int32* CPtr = IndexedUniquePositions.Find(VC);

				const uint32 A = APtr ? *APtr : IndexedUniquePositions.FindOrAdd(VA, Idx++);
				const uint32 B = BPtr ? *BPtr : IndexedUniquePositions.FindOrAdd(VB, Idx++);
				const uint32 C = CPtr ? *CPtr : IndexedUniquePositions.FindOrAdd(VC, Idx++);

				const uint32 AB = PCGEx::H64U(A, B);
				const uint32 BC = PCGEx::H64U(B, C);
				const uint32 AC = PCGEx::H64U(A, C);

				Edges.Add(AB);
				Edges.Add(BC);
				Edges.Add(AC);

				// Triangles and adjacency

				// Each edge can store its adjacency as a uint64 : left triangle index, right triangle index
				int32 TriangleIndex = Triangles.Add(UE::Geometry::FIndex3i(A, B, C));

				const uint64* AdjacencyABPtr = EdgeSides.Find(AB);
				const uint64* AdjacencyBCPtr = EdgeSides.Find(BC);
				const uint64* AdjacencyACPtr = EdgeSides.Find(AC);

				//Offset adjacency by 1 since we can't have -1 values
				if (AdjacencyABPtr) { EdgeSides.Add(AB, PCGEx::H64(TriangleIndex + 1, PCGEx::H64B(*AdjacencyABPtr))); }
				else { EdgeSides.FindOrAdd(AB, PCGEx::H64(0, TriangleIndex + 1)); }

				if (AdjacencyBCPtr) { EdgeSides.Add(BC, PCGEx::H64(TriangleIndex + 1, PCGEx::H64B(*AdjacencyBCPtr))); }
				else { EdgeSides.FindOrAdd(BC, PCGEx::H64(0, TriangleIndex + 1)); }

				if (AdjacencyACPtr) { EdgeSides.Add(AC, PCGEx::H64(TriangleIndex + 1, PCGEx::H64B(*AdjacencyACPtr))); }
				else { EdgeSides.FindOrAdd(AC, PCGEx::H64(0, TriangleIndex + 1)); }
			}

			Adjacencies.SetNumUninitialized(Triangles.Num());

			for (int j = 0; j < Adjacencies.Num(); j++)
			{
				UE::Geometry::FIndex3i Triangle = Triangles[j];

				const int32 A = PCGEx::H64NOT(*EdgeSides.Find(PCGEx::H64U(Triangle.A, Triangle.B)), j+1) - 1;
				const int32 B = PCGEx::H64NOT(*EdgeSides.Find(PCGEx::H64U(Triangle.B, Triangle.C)), j+1) - 1;
				const int32 C = PCGEx::H64NOT(*EdgeSides.Find(PCGEx::H64U(Triangle.A, Triangle.C)), j+1) - 1;

				Adjacencies[j] = UE::Geometry::FIndex3i(A, B, C);
			}

			EdgeSides.Empty();

			Vertices.SetNum(IndexedUniquePositions.Num());

			TArray<FVector> Keys;
			IndexedUniquePositions.GetKeys(Keys);
			for (FVector Key : Keys) { Vertices[*IndexedUniquePositions.Find(Key)] = Key; }

			IndexedUniquePositions.Empty();

			bIsLoaded = true;
		}

		void ExtractMeshAsync(FPCGExAsyncManager* AsyncManager)
		{
			if (bIsLoaded) { return; }
			if (!bIsValid) { return; }
			AsyncManager->Start<FExtractStaticMeshTask>(-1, nullptr, this);
		}

		~FGeoStaticMesh()
		{
		}
	};

	class PCGEXTENDEDTOOLKIT_API FGeoStaticMeshMap : public FGeoMesh
	{
	public:
		TMap<FSoftObjectPath, int32> Map;
		TArray<FGeoStaticMesh*> GSMs;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoStaticMeshMap()
		{
		}

		int32 Find(const FSoftObjectPath& InPath)
		{
			if (const int32* GSMPtr = Map.Find(InPath)) { return *GSMPtr; }

			FGeoStaticMesh* GSM = new FGeoStaticMesh(InPath);
			if (!GSM->bIsValid)
			{
				PCGEX_DELETE(GSM);
				return -1;
			}

			const int32 Index = GSMs.Add(GSM);
			GSM->DesiredTriangulationType = DesiredTriangulationType;
			Map.Add(InPath, Index);
			return Index;
		}

		FGeoStaticMesh* GetMesh(const int32 Index) { return GSMs[Index]; }

		~FGeoStaticMeshMap()
		{
			Map.Empty();
			PCGEX_DELETE_TARRAY(GSMs)
		}
	};

	class PCGEXTENDEDTOOLKIT_API FExtractStaticMeshTask : public FPCGExNonAbandonableTask
	{
	public:
		FExtractStaticMeshTask(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, FGeoStaticMesh* InGSM) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO), GSM(InGSM)
		{
		}

		FGeoStaticMesh* GSM = nullptr;

		virtual bool ExecuteTask() override
		{
			GSM->ExtractMeshSynchronous();
			return true;
		}
	};
}

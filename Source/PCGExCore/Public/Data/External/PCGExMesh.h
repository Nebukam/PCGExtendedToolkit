// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMeshCommon.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "UObject/SoftObjectPath.h"

struct FPCGExGeoMeshImportDetails;
struct FStreamableHandle;
struct FPCGExContext;

namespace PCGExMT
{
	class FTaskManager;
}

struct FPCGPinProperties;

namespace PCGExMesh
{
	PCGEXCORE_API
	void DeclareGeoMeshImportInputs(const FPCGExGeoMeshImportDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	struct PCGEXCORE_API FMeshData
	{
		FMeshData() = default;
		FMeshData(const UStaticMesh* InStaticMesh);

		int8 bIsValid = 0;
		int32 NumTexCoords = 0;
		FIndexArrayView Indices;
		const FStaticMeshVertexBuffers* Buffers = nullptr;
		const FPositionVertexBuffer* Positions = nullptr;
		const FColorVertexBuffer* Colors = nullptr;


		FORCEINLINE int32 NumTriangles() const { return Indices.Num() / 3; }
		FORCEINLINE int32 NumVertices() const { return Indices.Num(); }

		FORCEINLINE bool HasColor() const { return Colors != nullptr; }
	};

	class PCGEXCORE_API FMeshLookup : public TSharedFromThis<FMeshLookup>
	{
	protected:
		uint32 InternalIdx = 0;
		TArray<FVector>* Vertices = nullptr;
		TArray<int32>* RawIndices = nullptr;
		FVector HashTolerance = FVector(DefaultVertexMergeHashTolerance);
		bool bPreciseVertexMerge = true;
		/* This will be set false in the constructor if the hash tolerance is zero. */
		bool bEnableVertexMerge = true;

	public:
		/* Primary vertex hash */
		TMap<uint64, int32> Data;
		/* Secondary vertex hash used in precise merge mode */
		TMap<uint64, int32> DataOffset;

		explicit FMeshLookup(const int32 Size, TArray<FVector>* InVertices, TArray<int32>* InRawIndices,
			const FVector& InHashTolerance = FVector(DefaultVertexMergeHashTolerance), const bool bInPreciseVertexMerge = true);

		uint32 Add_GetIdx(const FVector& Position, const int32 RawIndex);

		/* Adds a vertex to the container and returns its index. */
		FORCEINLINE int32 AddVertex(const FVector& Position, const int32 RawIndex)
		{
			const int32 Idx = Vertices->Emplace(Position);
			if (RawIndices) { RawIndices->Emplace(RawIndex); }
			return Idx;
		}

		FORCEINLINE int32 Num() const { return Data.Num(); }
	};

	class FExtractStaticMeshTask;

	class PCGEXCORE_API FGeoMesh : public TSharedFromThis<FGeoMesh>
	{
	public:
		bool bIsValid = false;
		bool bIsLoaded = false;

		TArray<FVector> Vertices;
		TArray<int32> RawIndices;

		TSet<uint64> Edges;
		TArray<FIntVector3> Triangles;
		TArray<FIntVector3> Tri_Adjacency;
		TSet<int32> HullIndices;
		TSet<uint64> HullEdges;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoMesh() = default;
		~FGeoMesh() = default;

		void MakeDual();
		void MakeHollowDual();
	};

	class PCGEXCORE_API FGeoStaticMesh : public FGeoMesh
	{
	public:
		TObjectPtr<UStaticMesh> StaticMesh;
		FVector CWTolerance = FVector(DefaultVertexMergeHashTolerance);
		bool bPreciseVertexMerge = true;

		FMeshData RawData;

		explicit FGeoStaticMesh(const TSoftObjectPtr<UStaticMesh>& InSoftStaticMesh,
			const FVector& InCWTolerance = FVector(DefaultVertexMergeHashTolerance),
			const bool bInPreciseVertexMerge = true);
		explicit FGeoStaticMesh(const FSoftObjectPath& InSoftStaticMesh,
			const FVector& InCWTolerance = FVector(DefaultVertexMergeHashTolerance),
			const bool bInPreciseVertexMerge = true);
		explicit FGeoStaticMesh(const FString& InSoftStaticMesh,
			const FVector& InCWTolerance = FVector(DefaultVertexMergeHashTolerance),
			const bool bInPreciseVertexMerge = true);

		void ExtractMeshSynchronous();
		void TriangulateMeshSynchronous();

		void ExtractMeshAsync(PCGExMT::FTaskManager* TaskManager);

		~FGeoStaticMesh();

	protected:
		TSharedPtr<FStreamableHandle> MeshHandle;
	};

	class PCGEXCORE_API FGeoStaticMeshMap : public FGeoMesh
	{
	public:
		TMap<FSoftObjectPath, int32> Map;
		TArray<TSharedPtr<FGeoStaticMesh>> GSMs;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;
			
		FVector CWTolerance = FVector(DefaultVertexMergeHashTolerance);
		bool bPreciseVertexMerge = true;

		FGeoStaticMeshMap() = default;
		~FGeoStaticMeshMap() = default;

		int32 FindOrAdd(const FSoftObjectPath& InPath);

		TSharedPtr<FGeoStaticMesh> GetMesh(const int32 Index) { return GSMs[Index]; }
	};
}

// Copyright 2025 Timothé Lapetite and contributors
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
		FVector HashTolerance = FVector(0.001);

	public:
		TMap<uint64, int32> Data;

		explicit FMeshLookup(const int32 Size, TArray<FVector>* InVertices, TArray<int32>* InRawIndices, const FVector& InHashTolerance);

		uint32 Add_GetIdx(const FVector& Position, const int32 RawIndex);


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
		FVector CWTolerance = FVector(0.001);

		FMeshData RawData;

		explicit FGeoStaticMesh(const TSoftObjectPtr<UStaticMesh>& InSoftStaticMesh);
		explicit FGeoStaticMesh(const FSoftObjectPath& InSoftStaticMesh);
		explicit FGeoStaticMesh(const FString& InSoftStaticMesh);

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

		FGeoStaticMeshMap() = default;
		~FGeoStaticMeshMap() = default;

		int32 FindOrAdd(const FSoftObjectPath& InPath);

		TSharedPtr<FGeoStaticMesh> GetMesh(const int32 Index) { return GSMs[Index]; }
	};
}

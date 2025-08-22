// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExMT.h"

#include "PCGExGeoMesh.generated.h" // Credit goes to @Syscrusher attention to detail :D


UENUM()
enum class EPCGExTriangulationType : uint8
{
	Raw        = 0 UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual       = 1 UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow     = 2 UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
	Boundaries = 3 UMETA(DisplayName = "Boundaries", ToolTip="Outputs edges that are on the boundaries of the mesh (open spaces)"),
	NoTriangulation = 42 UMETA(Hidden),
};

namespace PCGExGeo
{
	class PCGEXTENDEDTOOLKIT_API FMeshLookup : public TSharedFromThis<FMeshLookup>
	{
	protected:
		uint32 InternalIdx = 0;
		TArray<FVector>* Vertices = nullptr;
		FVector HashTolerance = FVector(1 / 0.001);

	public:
		TMap<uint32, int32> Data;

		explicit FMeshLookup(const int32 Size, TArray<FVector>* InVertices, const FVector& InHashTolerance)
			: Vertices(InVertices), HashTolerance(InHashTolerance)
		{
			Data.Reserve(Size);
			InVertices->Reserve(Size);
		}

		uint32 Add_GetIdx(const FVector& Position);


		FORCEINLINE int32 Num() const { return Data.Num(); }
	};

	class FExtractStaticMeshTask;

	class PCGEXTENDEDTOOLKIT_API FGeoMesh : public TSharedFromThis<FGeoMesh>
	{
	public:
		bool bIsValid = false;
		bool bIsLoaded = false;
		TArray<FVector> Vertices;
		TSet<uint64> Edges;
		TArray<FIntVector3> Triangles;
		TArray<FIntVector3> Tri_Adjacency;
		TBitArray<> Tri_IsOnHull;
		TSet<int32> HullIndices;
		TSet<uint64> HullEdges;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoMesh() = default;
		~FGeoMesh() = default;

		void MakeDual();
		void MakeHollowDual();
	};

	class PCGEXTENDEDTOOLKIT_API FGeoStaticMesh : public FGeoMesh
	{
	public:
		TObjectPtr<UStaticMesh> StaticMesh;
		FVector CWTolerance = FVector(1 / 0.001);

		explicit FGeoStaticMesh(const TSoftObjectPtr<UStaticMesh>& InSoftStaticMesh);
		explicit FGeoStaticMesh(const FSoftObjectPath& InSoftStaticMesh);
		explicit FGeoStaticMesh(const FString& InSoftStaticMesh);

		void ExtractMeshSynchronous();
		void TriangulateMeshSynchronous();

		void ExtractMeshAsync(PCGExMT::FTaskManager* AsyncManager);

		~FGeoStaticMesh() = default;
	};

	class PCGEXTENDEDTOOLKIT_API FGeoStaticMeshMap : public FGeoMesh
	{
	public:
		TMap<FSoftObjectPath, int32> Map;
		TArray<TSharedPtr<FGeoStaticMesh>> GSMs;

		EPCGExTriangulationType DesiredTriangulationType = EPCGExTriangulationType::Raw;

		FGeoStaticMeshMap() = default;
		~FGeoStaticMeshMap() = default;

		int32 Find(const FSoftObjectPath& InPath);

		TSharedPtr<FGeoStaticMesh> GetMesh(const int32 Index) { return GSMs[Index]; }
	};

	class PCGEXTENDEDTOOLKIT_API FExtractStaticMeshTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FExtractStaticMeshTask)

		explicit FExtractStaticMeshTask(const TSharedPtr<FGeoStaticMesh>& InGSM);

		TSharedPtr<FGeoStaticMesh> GSM;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}

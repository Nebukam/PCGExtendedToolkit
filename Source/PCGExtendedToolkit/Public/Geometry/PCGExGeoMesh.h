// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "StaticMeshResources.h"


#include "PCGExGeoMesh.generated.h"

UENUM()
enum class EPCGExTriangulationType : uint8
{
	Raw    = 0 UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual   = 1 UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow = 2 UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
};

namespace PCGExGeo
{
	class PCGEXTENDEDTOOLKIT_API FMeshLookup : public TSharedFromThis<FMeshLookup>
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

	class PCGEXTENDEDTOOLKIT_API FGeoMesh : public TSharedFromThis<FGeoMesh>
	{
	public:
		bool bIsValid = false;
		bool bIsLoaded = false;
		TArray<FVector> Vertices;
		TSet<uint64> Edges;
		TArray<FIntVector3> Triangles;
		TArray<FIntVector3> Adjacencies;

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

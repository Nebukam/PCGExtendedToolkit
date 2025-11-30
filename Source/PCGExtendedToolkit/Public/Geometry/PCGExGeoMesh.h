// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "UObject/SoftObjectPath.h"
#include "Metadata/PCGMetadataCommon.h"

#include "PCGExMT.h"

#include "PCGExGeoMesh.generated.h" // Credit goes to @Syscrusher attention to detail :D

struct FPCGPinProperties;

UENUM()
enum class EPCGExTriangulationType : uint8
{
	Raw             = 0 UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual            = 1 UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow          = 2 UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
	Boundaries      = 3 UMETA(DisplayName = "Boundaries", ToolTip="Outputs edges that are on the boundaries of the mesh (open spaces)"),
	NoTriangulation = 42 UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGeoMeshImportDetails
{
	GENERATED_BODY()

	FPCGExGeoMeshImportDetails() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bImportVertexColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bImportUVs = false;

	/** A list of mapping channel in the format [Output Attribute Name]::[Channel Index]. You can output the same UV channel to different attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(DisplayName=" ├─ UV Channels Mapping", EditCondition="bImportUVs"))
	TMap<FName, int32> UVChannels;

	/** If enabled, will create placeholder attributes if a listed UV channel is missing. This is useful if the rest of your graph expects those attribute to exist, even if invalid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(DisplayName=" ├─ Create placeholders", EditCondition="bImportUVs"))
	bool bCreatePlaceholders = false;

	/** Placeholder UV value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(DisplayName=" └─ Placeholder Value", EditCondition="bImportUVs && bCreatePlaceholders"))
	FVector2D Placeholder = FVector2D(0, 0);

	TArray<FPCGAttributeIdentifier> UVChannelId;
	TArray<int32> UVChannelIndex;

	bool Validate(FPCGExContext* InContext);

	bool WantsImport() const;
};

namespace PCGExGeo
{
	const FName SourceUVImportRulesLabel = TEXT("UV Imports");

	PCGEXTENDEDTOOLKIT_API
	void DeclareGeoMeshImportInputs(const FPCGExGeoMeshImportDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	struct FMeshData
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

	class PCGEXTENDEDTOOLKIT_API FMeshLookup : public TSharedFromThis<FMeshLookup>
	{
	protected:
		uint32 InternalIdx = 0;
		TArray<FVector>* Vertices = nullptr;
		TArray<int32>* RawIndices = nullptr;
		FVector HashTolerance = FVector(1 / 0.001);

	public:
		TMap<uint64, int32> Data;

		explicit FMeshLookup(const int32 Size, TArray<FVector>* InVertices, TArray<int32>* InRawIndices, const FVector& InHashTolerance);

		uint32 Add_GetIdx(const FVector& Position, const int32 RawIndex);


		FORCEINLINE int32 Num() const { return Data.Num(); }
	};

	class FExtractStaticMeshTask;

	class PCGEXTENDEDTOOLKIT_API FGeoMesh : public TSharedFromThis<FGeoMesh>
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

		bool bHasColorData = false;

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

		FMeshData RawData;

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

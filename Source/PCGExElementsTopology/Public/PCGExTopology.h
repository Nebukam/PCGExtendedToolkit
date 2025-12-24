// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Descriptors/PCGExComponentDescriptors.h"
#include "Data/Descriptors/PCGExDynamicMeshDescriptor.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"

#include "PCGExTopology.generated.h"

struct FPCGExContext;
struct FGeometryScriptSimplePolygon;

namespace PCGExData
{
	class FFacadePreloader;
	class FFacade;
	struct FMutablePoint;

	template <typename T>
	class TBuffer;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExMath
{
	namespace Geo
	{
		struct FTriangle;
	}

	struct FTriangle;
}

namespace PCGExClusters
{
	class FCluster;
}

UENUM()
enum class EPCGExTopologyOutputType : uint8
{
	PerItem = 1 UMETA(DisplayName = "Per-item Geometry", Tooltip="Output a geometry object per-item"),
	Merged  = 0 UMETA(DisplayName = "Merged Geometry", Tooltip="Output a single geometry that merges all generated topologies"),
};

namespace PCGExTopology
{
	const FName MeshOutputLabel = TEXT("Mesh");
}

USTRUCT(BlueprintType)
struct PCGEXELEMENTSTOPOLOGY_API FPCGExUVInputDetails
{
	GENERATED_BODY()

	FPCGExUVInputDetails() = default;

	/** Whether this input is enabled or not */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bEnabled = true;

	/** Name of the attribute containing the UVs (expects FVector2) */
	UPROPERTY(EditAnywhere, Category = Settings)
	FName AttributeName = NAME_None;

	/** Index of the UV channel on the final model */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin="0", ClampMax="7", UIMin="0", UIMax="7"))
	int32 Channel = 0;
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSTOPOLOGY_API FPCGExTopologyUVDetails
{
	GENERATED_BODY()

	FPCGExTopologyUVDetails() = default;

	/** List of UV channels */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExUVInputDetails> UVs;

	int32 NumChannels = 0;
	TArray<int32> ChannelIndices;
	TArray<TSharedPtr<PCGExData::TBuffer<FVector2D>>> UVBuffers;

	void Prepare(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;
	void Write(const TArray<int32>& TriangleIDs, UE::Geometry::FDynamicMesh3& InMesh) const;
	void Write(const TArray<int32>& TriangleIDs, const TArray<int32>& VtxIDs, UE::Geometry::FDynamicMesh3& InMesh) const;
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSTOPOLOGY_API FPCGExTopologyDetails
{
	GENERATED_BODY()

	FPCGExTopologyDetails()
	{
	}

	/** Default material assigned to the mesh */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Default vertex color used for the points. Will use point color when available.*/
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FLinearColor DefaultVertexColor = FLinearColor::White;

	/** UV input settings */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExTopologyUVDetails UVChannels;

	/** Default primitive options
	 * Note that those are applied when triangulation is appended to the dynamic mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPrimitiveOptions PrimitiveOptions;

	/** Triangulation options
	 * Note that those are applied when triangulation is appended to the dynamic mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPolygonsTriangulationOptions TriangulationOptions;

	/** If enabled, will not throw an error in case Geometry Script complain about bad triangulation.
	 * If it shows, something went wrong but it's impossible to know exactly why. Look for structural anomalies, overlapping points, ...*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bQuietTriangulationError = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWeldEdges = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, EditCondition="bWeldEdges"))
	FGeometryScriptWeldEdgesOptions WeldEdgesOptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bComputeNormals = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, EditCondition="bComputeNormals"))
	FGeometryScriptCalculateNormalsOptions NormalsOptions;

	/** Dynamic mesh component data. Only used by legacy output mode that spawned the component, prior to vanilla PCG interop.
	 * This will be completely ignored in most usecases. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable), AdvancedDisplay)
	FPCGExDynamicMeshDescriptor TemplateDescriptor;

	void PostProcessMesh(const TObjectPtr<UDynamicMesh>& InDynamicMesh) const;
};

namespace PCGExTopology
{
	namespace Labels
	{
		const FName SourceMeshLabel = FName("Mesh");
		const FName OutputMeshLabel = FName("Mesh");
	}

	PCGEXELEMENTSTOPOLOGY_API void MarkTriangle(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const PCGExMath::Geo::FTriangle& InTriangle);
}

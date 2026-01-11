// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTopology.h"
#include "Core/PCGExClipper2Processor.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Triangulate.generated.h"

class UDynamicMesh;
class UPCGDynamicMeshData;

/**
 * Clipper2 Constrained Delaunay Triangulation
 * Converts closed paths into a triangulated mesh, preserving source point indices for attribute lookup.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/clipper2/clipper2-triangulate"))
class UPCGExClipper2TriangulateSettings : public UPCGExClipper2ProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Clipper2Triangulate, "Clipper2 : Triangulate", "Performs Constrained Delaunay Triangulation on closed paths and outputs a Dynamic Mesh.");
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Projection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2FillRule FillRule = EPCGExClipper2FillRule::EvenOdd;
	
	/** Use Delaunay optimization for better triangle quality */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseDelaunay = true;

	/** Attempt to repair degenerate geometry after triangulation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mesh", meta = (PCG_Overridable))
	bool bAttemptRepair = false;

	/** Repair options for degenerate geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mesh", meta = (PCG_Overridable, EditCondition="bAttemptRepair", EditConditionHides))
	FGeometryScriptDegenerateTriangleOptions RepairDegenerate;

	/** Topology settings. Some settings will be ignored based on selected output mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mesh")
	FPCGExTopologyDetails Topology;
	
	/** Suppress warnings about bad/duplicate vertices */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta = (PCG_Overridable))
	bool bQuietBadVerticesWarning = false;

	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
	virtual bool SupportOpenMainPaths() const override { return false; } // Triangulation requires closed paths
};

/**
 * Internal vertex data during triangulation
 */
struct FPCGExTriangulationVertex
{
	FVector Position;       // Unprojected 3D position
	FVector4 Color;         // Vertex color from source
	int32 SourceDataIndex;  // Which input path (-1 if unknown)
	int32 SourcePointIndex; // Which point in that path (-1 if new/interpolated)
	int64 ClipperX;         // Original Clipper2 X coordinate (for matching)
	int64 ClipperY;         // Original Clipper2 Y coordinate (for matching)

	FPCGExTriangulationVertex()
		: Position(FVector::ZeroVector)
		  , Color(FVector4(1, 1, 1, 1))
		  , SourceDataIndex(-1)
		  , SourcePointIndex(-1)
		  , ClipperX(0)
		  , ClipperY(0)
	{
	}
};

struct FPCGExClipper2TriangulateContext final : FPCGExClipper2ProcessorContext
{
	friend class FPCGExClipper2TriangulateElement;

	// Mesh output
	TObjectPtr<UPCGDynamicMeshData> MeshData;
	TObjectPtr<UDynamicMesh> Mesh;

	// Accumulated vertex and triangle data across all groups
	TArray<FPCGExTriangulationVertex> VertexPool;
	TArray<FIntVector> Triangles;
	TMap<uint64, int32> VertexMap; // Hash(x,y) -> pool index

	// Tag accumulator for output
	TSharedPtr<PCGExData::FTags> OutputTags;

	virtual void Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group) override;

	// Build the final mesh from accumulated data
	void BuildMesh();

protected:
	// Build vertex pool from input paths, tracking source indices
	void BuildVertexPoolFromGroup(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group);

	// Find vertex in pool by Clipper2 coordinates, returns index or -1
	int32 FindVertexIndex(int64 X, int64 Y) const;

	// Hash a Clipper2 point for fast lookup
	static uint64 HashPoint(int64 X, int64 Y);
};

class FPCGExClipper2TriangulateElement final : public FPCGExClipper2ProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Triangulate)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void OutputWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

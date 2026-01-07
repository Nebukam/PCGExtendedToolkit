// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMeshCommon.generated.h"

UENUM()
enum class EPCGExTriangulationType : uint8
{
	Raw             = 0 UMETA(DisplayName = "Raw Triangles", ToolTip="Make a graph from raw triangles."),
	Dual            = 1 UMETA(DisplayName = "Dual Graph", ToolTip="Dual graph of the triangles (using triangle centroids and adjacency)."),
	Hollow          = 2 UMETA(DisplayName = "Hollow Graph", ToolTip="Connects centroid to vertices but remove triangles edges"),
	Boundaries      = 3 UMETA(DisplayName = "Boundaries", ToolTip="Outputs edges that are on the boundaries of the mesh (open spaces)"),
	NoTriangulation = 42 UMETA(Hidden),
};

namespace PCGExMesh::Labels
{
	const FName SourceUVImportRulesLabel = TEXT("UV Imports");
}

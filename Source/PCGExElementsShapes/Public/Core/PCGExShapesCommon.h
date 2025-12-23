// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExShapesCommon.generated.h"

UENUM()
enum class EPCGExShapeOutputMode : uint8
{
	PerDataset = 0 UMETA(DisplayName = "Per Dataset", ToolTip="Merge all shapes into the original dataset"),
	PerSeed    = 1 UMETA(DisplayName = "Per Seed", ToolTip="Create a new output per shape seed point"),
	PerShape   = 2 UMETA(DisplayName = "Per Shape", ToolTip="Create a new output per individual shape"),
};


UENUM()
enum class EPCGExShapeResolutionMode : uint8
{
	Absolute = 0 UMETA(DisplayName = "Absolute", ToolTip="Resolution is absolute."),
	Scaled   = 1 UMETA(DisplayName = "Scaled", ToolTip="Resolution is scaled by the seed' scale"),
};

UENUM()
enum class EPCGExShapePointLookAt : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="Point look at will be as per 'canon' shape definition"),
	Seed = 1 UMETA(DisplayName = "Seed", ToolTip="Look At Seed"),
};

namespace PCGExShapes::Labels
{
	const FName OutputShapeBuilderLabel = TEXT("Shape Builder");
	const FName SourceShapeBuildersLabel = TEXT("Shape Builders");
}

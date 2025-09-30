// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExDetailsNoise.generated.h"

#pragma region PCG exposition
// Exposed copy of the otherwise private PCG' spatial noise mode enum
UENUM(BlueprintType)
enum class PCGExSpatialNoiseMode : uint8
{
	/** Your classic perlin noise. */
	Perlin,
	/** Based on underwater fake caustic rendering, gives swirly look. */
	Caustic,
	/** Voronoi noise, result a the distance to edge and cell ID. */
	Voronoi,
	/** Based on fractional brownian motion. */
	FractionalBrownian,
	/** Used to create masks to blend out edges. */
	EdgeMask,
};

UENUM(BlueprintType)
enum class PCGExSpatialNoiseMask2DMode : uint8
{
	/** Your classic perlin noise. */
	Perlin,
	/** Based on underwater fake caustic rendering, gives swirly look. */
	Caustic,
	/** Based on fractional brownian motion. */
	FractionalBrownian,
};

#pragma endregion

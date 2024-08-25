// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMath.h"
#include "Data/PCGExPointIO.h"

namespace PCGExRandom
{
	FORCEINLINE static int ComputeSeed(int A)
	{
		// From Epic git main, unexposed in 5.3
		return (A * 196314165U) + 907633515U;
	}

	FORCEINLINE static int ComputeSeed(int A, int B)
	{
		// From Epic git main, unexposed in 5.3
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U);
	}

	FORCEINLINE static int ComputeSeed(int A, int B, int C)
	{
		// From Epic git main, unexposed in 5.3
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U) ^ ((C * 34731343U) + 453816743U);
	}

	FORCEINLINE static FRandomStream GetRandomStreamFromPoint(const FPCGPoint& Point, const int32 Offset, const UPCGSettings* Settings = nullptr, const UPCGComponent* Component = nullptr)
	{
		// From Epic git main, unexposed in 5.3

		int Seed = Point.Seed + Offset;

		if (Settings && Component) { Seed = ComputeSeed(Seed, Settings->Seed, Component->Seed); }
		else if (Settings) { Seed = ComputeSeed(Seed, Settings->Seed); }
		else if (Component) { Seed = ComputeSeed(Seed, Component->Seed); }

		return FRandomStream(Seed);
	}

	FORCEINLINE static int ComputeSeed(const FPCGPoint& Point, const FVector& Offset = FVector::ZeroVector)
	{
		return static_cast<int>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Point.Transform.GetLocation() * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	}
}

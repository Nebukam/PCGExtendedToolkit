// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExTestHelpers.h"

namespace PCGExTest
{
	TArray<FVector> GenerateRandomPositions(int32 NumPoints, const FBox& Bounds, uint32 Seed)
	{
		TArray<FVector> Positions;
		Positions.Reserve(NumPoints);

		FRandomStream Random(Seed);
		const FVector Size = Bounds.GetSize();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Position(
				Bounds.Min.X + Random.FRand() * Size.X,
				Bounds.Min.Y + Random.FRand() * Size.Y,
				Bounds.Min.Z + Random.FRand() * Size.Z
			);
			Positions.Add(Position);
		}

		return Positions;
	}

	TArray<FVector> GenerateGridPositions(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		TArray<FVector> Positions;
		Positions.Reserve(CountX * CountY * CountZ);

		for (int32 Z = 0; Z < CountZ; ++Z)
		{
			for (int32 Y = 0; Y < CountY; ++Y)
			{
				for (int32 X = 0; X < CountX; ++X)
				{
					FVector Position = Origin + FVector(
						X * Spacing.X,
						Y * Spacing.Y,
						Z * Spacing.Z
					);
					Positions.Add(Position);
				}
			}
		}

		return Positions;
	}

	TArray<FVector> GenerateSpherePositions(
		const FVector& Center,
		double Radius,
		int32 NumPoints,
		uint32 Seed)
	{
		TArray<FVector> Positions;
		Positions.Reserve(NumPoints);

		FRandomStream Random(Seed);

		for (int32 i = 0; i < NumPoints; ++i)
		{
			// Generate uniformly distributed points on sphere surface
			// Using spherical coordinates with proper distribution
			const double Theta = 2.0 * PI * Random.FRand();
			const double Phi = FMath::Acos(1.0 - 2.0 * Random.FRand());

			FVector Position(
				Center.X + Radius * FMath::Sin(Phi) * FMath::Cos(Theta),
				Center.Y + Radius * FMath::Sin(Phi) * FMath::Sin(Theta),
				Center.Z + Radius * FMath::Cos(Phi)
			);
			Positions.Add(Position);
		}

		return Positions;
	}
}

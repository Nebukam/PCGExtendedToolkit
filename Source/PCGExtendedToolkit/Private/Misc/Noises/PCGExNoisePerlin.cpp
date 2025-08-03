// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Misc/Noises/PCGExNoisePerlin.h"

namespace PCGExNoise
{
	FPerlinNoise::FPerlinNoise(const FTransform& InTransform)
		: INoiseGenerator(InTransform)
	{
	}

	float FPerlinNoise::Compute(const FTransform& InTransform) const
	{
		FVector Sample = Transform.TransformPosition(InTransform.GetLocation()) * Frequency;
		
		double X = Sample.X;
		double Y = Sample.Y;
		double Z = Sample.Z;

		int32 cx = FMath::FloorToInt32(X);
		int32 cy = FMath::FloorToInt32(Y);
		int32 cz = FMath::FloorToInt32(Z);

		X -= cx;
		Y -= cy;
		Z -= cz;

		var u = Fade(X);
		var v = Fade(Y);
		var w = Fade(Z);

		var h000 = Hash(CellID(cx    , cy    , cz    ));
		var h001 = Hash(CellID(cx + 1, cy    , cz    ));
		var h010 = Hash(CellID(cx    , cy + 1, cz    ));
		var h011 = Hash(CellID(cx + 1, cy + 1, cz    ));
		var h100 = Hash(CellID(cx    , cy    , cz + 1));
		var h101 = Hash(CellID(cx + 1, cy    , cz + 1));
		var h110 = Hash(CellID(cx    , cy + 1, cz + 1));
		var h111 = Hash(CellID(cx + 1, cy + 1, cz + 1));

		var n = Lerp(w, Lerp(v, Lerp(u, Grad(h000, X, Y  , Z  ), Grad(h001, X-1, Y  , Z  )),
								Lerp(u, Grad(h010, X, Y-1, Z  ), Grad(h011, X-1, Y-1, Z  ))),
						Lerp(v, Lerp(u, Grad(h100, X, Y  , Z-1), Grad(h101, X-1, Y  , Z-1)),
								Lerp(u, Grad(h110, X, Y-1, Z-1), Grad(h111, X-1, Y-1, Z-1))));
		return n * 0.5f + 0.5f;
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExNoise.h"

namespace PCGExNoise
{
	class FPerlinNoise : public INoiseGenerator
	{
	public:
		FPerlinNoise(const FTransform& InTransform = FTransform::Identity);
		virtual float Compute(const FTransform& InTransform) const;

	protected:
		 double Frequency = 0;
		 int32 Repeat = 0;
		 int32 Seed = 0;

        FORCEINLINE static float Fade(const float T)        {            return T * T * T * (T * (T * 6 - 15) + 10);        }

        static float Lerp(float T, float A, float B)        {            return A + T * (B - A);        }

        static float Grad(int H, float X, float Y)        {            return ((H & 1) == 0 ? X : -X) + ((H & 2) == 0 ? Y : -Y);        }

        static float Grad(const int Hash, const float X, const float Y, const float Z)
        {
            int32 H = Hash & 15;
            float U = H < 8 ? X : Y;
            float V = H < 4 ? Y : (H == 12 || H == 14 ? X : Z);
            return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
        }

	};
}

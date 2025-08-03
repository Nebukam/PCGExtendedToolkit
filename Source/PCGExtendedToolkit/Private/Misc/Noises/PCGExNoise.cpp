// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Misc/Noises/PCGExNoise.h"

namespace PCGExNoise
{
	INoiseGenerator::INoiseGenerator(const FTransform& InTransform)
		: Transform(InTransform)
	{
	}

	float INoiseGenerator::Compute(const FTransform& InTransform) const
	{
		return 0;
	}
}

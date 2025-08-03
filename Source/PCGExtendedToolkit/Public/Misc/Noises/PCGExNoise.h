// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"

namespace PCGExNoise
{
	class INoiseGenerator : public TSharedFromThis<INoiseGenerator>
	{
	protected:
		FTransform Transform = FTransform::Identity;

	public:
		INoiseGenerator(const FTransform& InTransform = FTransform::Identity);
		virtual float Compute(const FTransform& InTransform) const;
	};
}

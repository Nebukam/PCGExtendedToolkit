// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExShape.h"

#include "Core/PCGExShapeConfigBase.h"

namespace PCGExShapes
{
	void FShape::ComputeFit(const FPCGExShapeConfigBase& Config)
	{
		Fit = FBox(FVector::OneVector * -0.5, FVector::OneVector * 0.5);
		FTransform OutTransform = FTransform::Identity;

		Config.Fitting.ComputeTransform(Seed.Index, OutTransform, Fit, false);

		Fit = Fit.TransformBy(OutTransform);
		Fit = Fit.TransformBy(Config.LocalTransform);

		Extents = Config.DefaultExtents;
	}
}

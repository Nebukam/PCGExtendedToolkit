// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExPointElements.h"

struct FPCGExShapeConfigBase;

namespace PCGExShapes
{
	class PCGEXELEMENTSSHAPES_API FShape : public TSharedFromThis<FShape>
	{
	public:
		virtual ~FShape() = default;
		PCGExData::FConstPoint Seed;

		int32 StartIndex = 0;
		int32 NumPoints = 0;
		int8 bValid = 1;

		FBox Fit = FBox(ForceInit);
		FVector Extents = FVector::OneVector * 50;

		bool IsValid() const { return bValid && Fit.IsValid && NumPoints > 0; }

		explicit FShape(const PCGExData::FConstPoint& InPointRef)
			: Seed(InPointRef)
		{
		}

		virtual void ComputeFit(const FPCGExShapeConfigBase& Config);
	};
}

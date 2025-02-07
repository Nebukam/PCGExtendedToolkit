// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Layout/PCGExLayout.h"

namespace PCGExLayout
{
	bool CanBoxFit(const FBox& InBox, const FVector& InSize)
	{
		const FVector BoxSize = InBox.GetSize();

		return (InSize.X <= BoxSize.X &&
				InSize.Y <= BoxSize.Y &&
				InSize.Z <= BoxSize.Z);
	}
}

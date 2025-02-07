// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

//#include "PCGExLayout.generated.h"

namespace PCGExLayout
{
	const FName SourceBinsLabel = TEXT("Bins");
	const FName OutputBinsLabel = TEXT("Bins");
	const FName OutputDiscardedLabel = TEXT("Discarded");

	bool CanBoxFit(const FBox& InBox, const FVector& InSize);
}

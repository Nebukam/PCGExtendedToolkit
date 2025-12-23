// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExData
{
	class FAttributesInfos;
	class FPointIO;
}

namespace PCGExBlending::Helpers
{
	PCGEXBLENDING_API
	void MergeBestCandidatesAttributes(
		const TSharedPtr<PCGExData::FPointIO>& Target,
		const TArray<TSharedPtr<PCGExData::FPointIO>>& Collections,
		const TArray<int32>& BestIndices,
		const PCGExData::FAttributesInfos& InAttributesInfos);
}

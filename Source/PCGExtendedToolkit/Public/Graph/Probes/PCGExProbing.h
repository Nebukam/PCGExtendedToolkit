// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

namespace PCGExProbing
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		int32 PointIndex;
		FVector Direction;
		double Distance;
		uint32 GH;

		FCandidate(const int32 InIndex, const FVector& InDirection, const double InDistance, const uint32 InGH):
			PointIndex(InIndex), Direction(InDirection), Distance(InDistance), GH(InGH)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FBestCandidate
	{
		int32 BestIndex = -1;
		double BestPrimaryValue = 0;
		double BestSecondaryValue = 0;

		FBestCandidate()
		{
		}
	};
}

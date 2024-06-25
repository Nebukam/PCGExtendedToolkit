// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

namespace PCGExProbing
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		int32 Index;
		FVector Direction;
		double Distance;

		FCandidate(const int32 InIndex, const FVector& InDirection, const double InDistance):
			Index(InIndex), Direction(InDirection), Distance(InDistance)
		{
		}
	};
}

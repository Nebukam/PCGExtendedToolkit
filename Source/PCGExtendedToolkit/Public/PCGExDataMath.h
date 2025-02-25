// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "Data/PCGExData.h"

//#include "PCGExDataMath.generated.h"

namespace PCGExMath
{
	PCGEXTENDEDTOOLKIT_API
	FVector NRM(
		const int32 A, const int32 B, const int32 C,
		const TArray<FVector>& InPositions,
		const PCGExData::TBuffer<FVector>* UpVectorCache = nullptr,
		const FVector& UpVector = FVector::UpVector);
}

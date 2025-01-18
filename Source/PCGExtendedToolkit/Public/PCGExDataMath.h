// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "Data/PCGExData.h"

//#include "PCGExDataMath.generated.h"

namespace PCGExMath
{
	FORCEINLINE static FVector NRM(
		const int32 A, const int32 B, const int32 C,
		const TArray<FVector>& InPositions, const PCGExData::TBuffer<FVector>* UpVectorCache = nullptr, const FVector& UpVector = FVector::UpVector)
	{
		const FVector VA = *(InPositions.GetData() + A);
		const FVector VB = *(InPositions.GetData() + B);
		const FVector VC = *(InPositions.GetData() + C);

		FVector UpAverage = UpVector;
		if (UpVectorCache)
		{
			UpAverage += UpVectorCache->Read(A);
			UpAverage += UpVectorCache->Read(B);
			UpAverage += UpVectorCache->Read(C);
			UpAverage /= 3;
			UpAverage = UpAverage.GetSafeNormal();
		}

		return FMath::Lerp(GetNormal(VA, VB, VB + UpAverage), GetNormal(VB, VC, VC + UpAverage), 0.5).GetSafeNormal();
	}
}

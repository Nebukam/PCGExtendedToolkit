// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDataMath.h"

namespace PCGExMath
{
	FVector NRM(
		const int32 A, const int32 B, const int32 C,
		const TArray<FVector>& InPositions,
		const PCGExData::TBuffer<FVector>* UpVectorCache,
		const FVector& UpVector)
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

	FBox GetLocalBounds(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		case EPCGExPointBoundsSource::ScaledBounds: return GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point);
		case EPCGExPointBoundsSource::DensityBounds: return GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point);
		case EPCGExPointBoundsSource::Bounds: return GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point);
		case EPCGExPointBoundsSource::Center: return GetLocalBounds<EPCGExPointBoundsSource::Center>(Point);
		default: return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMathBounds.h"
#include "Data/PCGExData.h"

namespace PCGExMath
{
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

	FBox GetLocalBounds(const PCGExData::FProxyPoint& Point, const EPCGExPointBoundsSource Source)
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

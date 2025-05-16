// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointData.h"
#include "Data/PCGExPointIO.h"


UPCGSpatialData* UPCGExPointData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_NEW_CUSTOM_POINT_DATA(UPCGExPointData)
	return NewData;
}

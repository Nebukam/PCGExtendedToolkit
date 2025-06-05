// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointData.h"
#include "Data/PCGExPointIO.h"


UPCGSpatialData* UPCGExPointData::CopyInternal(FPCGContext* Context) const
{
	UPCGExPointData* NewData = Cast<UPCGExPointData>(FPCGContext::NewObject_AnyThread<UPCGExPointData>(Context));
	if (!SupportsSpatialDataInheritance()) { NewData->CopyUnallocatedPropertiesFrom(this); }
	return NewData;
}

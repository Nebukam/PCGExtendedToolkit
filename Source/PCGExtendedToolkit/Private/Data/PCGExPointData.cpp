// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointData.h"

#include "Data/PCGExPointIO.h"

void UPCGExPointData::CopyFrom(const UPCGPointData* InPointData)
{
	GetMutablePoints() = InPointData->GetPoints();
	InitializeFromData(InPointData);
	if (const UPCGExPointData* TypedData = Cast<UPCGExPointData>(InPointData))
	{
		InitializeFromPCGExData(TypedData, PCGExData::EIOInit::Duplicate);
	}
}

void UPCGExPointData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode)
{
}

void UPCGExPointData::BeginDestroy()
{
	Super::BeginDestroy();
}

PCGEX_DATA_COPY_INTERNAL_IMPL(UPCGExPointData)

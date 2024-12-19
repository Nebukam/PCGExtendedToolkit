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

#if PCGEX_ENGINE_VERSION < 505
UPCGSpatialData* UPCGExPointData::CopyInternal() const
{
	UPCGExPointData* NewPointData = nullptr;
	{
		FGCScopeGuard GCGuard;
		NewObject<UPCGExPointData>();
	}
	NewPointData->CopyFrom(this);
	return NewPointData;
}
#else
UPCGSpatialData* UPCGExPointData::CopyInternal(FPCGContext* Context) const
{
	UPCGExPointData* NewPointData = FPCGContext::NewObject_AnyThread<UPCGExPointData>(Context);
	NewPointData->CopyFrom(this);
	return NewPointData;
}
#endif

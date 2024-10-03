// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExPointData.h"

#include "Data/PCGExPointIO.h"

void UPCGExPointData::CopyFrom(const UPCGPointData* InPointData)
{
	GetMutablePoints() = InPointData->GetPoints();
	InitializeFromData(InPointData);
	if (const UPCGExPointData* TypedData = Cast<UPCGExPointData>(InPointData))
	{
		InitializeFromPCGExData(TypedData, PCGExData::EInit::DuplicateInput);
	}
}

void UPCGExPointData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode)
{
}

void UPCGExPointData::BeginDestroy()
{
	Super::BeginDestroy();
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
UPCGSpatialData* UPCGExPointData::CopyInternal() const
{
	NewPointData = nullptr;
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

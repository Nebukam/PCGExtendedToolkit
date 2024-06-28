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
	//UE_LOG(LogTemp, Warning, TEXT("RELEASE UPCGExPointData"))
}

UPCGSpatialData* UPCGExPointData::CopyInternal() const
{
	UPCGExPointData* NewPointData = NewObject<UPCGExPointData>();
	NewPointData->CopyFrom(this);
	return NewPointData;
}

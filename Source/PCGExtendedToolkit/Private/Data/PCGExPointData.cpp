// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExPointData.h"

UPCGSpatialData* UPCGExPointData::CopyInternal() const
{
	UPCGExPointData* NewPointData = NewObject<UPCGExPointData>();
	NewPointData->GetMutablePoints() = GetPoints();

	return NewPointData;
}

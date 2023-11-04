// Fill out your copyright notice in the Description page of Project Settings.


#include "Relational/PCGExRelationalData.h"

#include "Data/PCGPointData.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExRelationalData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

const TArray<FPCGExRelationalSlot>& UPCGExRelationalData::GetConstSlots()
{
	return Slots;
}

void UPCGExRelationalData::InitializeLocalDefinition(const FPCGExRelationsDefinition& Definition)
{
	GreatestMaxDistance = 0.0;
	Slots.Reset();
	for (const FPCGExRelationalSlot& Slot : Definition.Slots)
	{
		Slots.Add(Slot);
		GreatestMaxDistance = FMath::Max(GreatestMaxDistance, Slot.Direction.MaxDistance);
	}
}

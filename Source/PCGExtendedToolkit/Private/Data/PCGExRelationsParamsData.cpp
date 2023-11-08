// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExRelationsParamsData.h"
#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationsParamsData::UPCGExRelationsParamsData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

/**
 * 
 * @param PointData 
 * @return 
 */
bool UPCGExRelationsParamsData::HasMatchingRelationsData(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	for (const PCGExRelational::FSocket Socket : SocketMapping.Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.AttributeName)) { return false; }
	}
	return true;
}

void UPCGExRelationsParamsData::InitializeSockets(TArray<FPCGExSocketDescriptor>& InSockets)
{
	SocketMapping.Initialize(RelationIdentifier, InSockets);

	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;
	
	for (const FPCGExSocketDescriptor& Socket : InSockets)
	{
		if (!Socket.bEnabled) { continue; }
		if (Socket.bApplyAttributeModifier) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Socket.Direction.MaxDistance);
	}
	
}

/**
 * Prepare socket mapping for working with a given PointData object.
 * @param PointData 
 */
void UPCGExRelationsParamsData::PrepareForPointData(UPCGPointData* PointData)
{
	SocketMapping.PrepareForPointData(PointData);
}

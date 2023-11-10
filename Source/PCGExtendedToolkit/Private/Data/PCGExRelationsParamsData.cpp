// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExRelationsParamsData.h"
#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationsParamsData::UPCGExRelationsParamsData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExRelationsParamsData::HasMatchingRelationsData(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	for (const PCGExRelational::FSocket Socket : SocketMapping.Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.GetName())) { return false; }
	}
	return true;
}

void UPCGExRelationsParamsData::InitializeSockets(
	TArray<FPCGExSocketDescriptor>& InSockets,
	const bool bApplyOverrides,
	FPCGExSocketGlobalOverrides& Overrides)
{
	SocketMapping = PCGExRelational::FSocketMapping{};

	if (bApplyOverrides) { SocketMapping.InitializeWithOverrides(RelationIdentifier, InSockets, Overrides); }
	else { SocketMapping.Initialize(RelationIdentifier, InSockets); }

	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;

	for (const FPCGExSocketDescriptor& Socket : InSockets)
	{
		if (!Socket.bEnabled) { continue; }
		if (Socket.bApplyAttributeModifier) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Socket.Direction.MaxDistance);
	}
}

void UPCGExRelationsParamsData::PrepareForPointData(UPCGPointData* PointData)
{
	SocketMapping.PrepareForPointData(PointData);
}

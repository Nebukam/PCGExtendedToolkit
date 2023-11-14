// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExRelationsParamsData.h"
#include "Data/PCGPointData.h"

struct FPCGExRelationsProcessorContext;

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

void UPCGExRelationsParamsData::Initialize(
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

	CachedIndexAttributeName = SocketMapping.GetCompoundName(FName("CachedIndex"));
}

void UPCGExRelationsParamsData::PrepareForPointData(FPCGExRelationsProcessorContext* Context, UPCGPointData* PointData)
{
	Context->CachedIndex = PointData->Metadata->FindOrCreateAttribute<int64>(Context->CurrentParams->CachedIndexAttributeName,-1,false);
	SocketMapping.PrepareForPointData(PointData);
}

void UPCGExRelationsParamsData::GetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExRelational::FSocketMetadata>& OutMetadata) const
{
	OutMetadata.Reset(SocketMapping.NumSockets);
	for (const PCGExRelational::FSocket& Socket : SocketMapping.Sockets) { OutMetadata.Add(Socket.GetData(MetadataEntry)); }
}

void UPCGExRelationsParamsData::SetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExRelational::FSocketMetadata>& InMetadata)
{
	check(InMetadata.Num() == SocketMapping.NumSockets)
	for (int i = 0; i < SocketMapping.NumSockets; i++)
	{
		PCGExRelational::FSocket& Socket = SocketMapping.Sockets[i];
		Socket.SetData(MetadataEntry, InMetadata[i]);
	}
}

void UPCGExRelationsParamsData::GetSocketsInfos(TArray<PCGExRelational::FSocketInfos>& OutInfos)
{
	SocketMapping.GetSocketsInfos(OutInfos);
}

﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGraphParamsData.h"
#include "Data/PCGPointData.h"

struct PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorContext;

UPCGExGraphParamsData::UPCGExGraphParamsData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExGraphParamsData::HasMatchingGraphData(const UPCGPointData* PointData) const
{
	// Whether the data has metadata matching this GraphData block or not
	for (const PCGExGraph::FSocket Socket : SocketMapping->Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.GetName())) { return false; }
	}
	return true;
}

void UPCGExGraphParamsData::BeginDestroy()
{
	Cleanup();

	PCGEX_DELETE(SocketMapping);
	SocketsDescriptors.Empty();
	Super::BeginDestroy();
}

void UPCGExGraphParamsData::Initialize()
{
	SocketMapping = new PCGExGraph::FSocketMapping();

	if (bApplyGlobalOverrides) { SocketMapping->InitializeWithOverrides(GraphIdentifier, SocketsDescriptors, GlobalOverrides); }
	else { SocketMapping->Initialize(GraphIdentifier, SocketsDescriptors); }

	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;

	for (const FPCGExSocketDescriptor& Socket : SocketsDescriptors)
	{
		if (!Socket.bEnabled) { continue; }
		if (Socket.bApplyAttributeModifier) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Socket.Bounds.MaxDistance);
	}

	CachedIndexAttributeName = SocketMapping->GetCompoundName(FName("CachedIndex"));
}

void UPCGExGraphParamsData::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly = true) const
{
	SocketMapping->PrepareForPointData(PointIO, bReadOnly);
}

void UPCGExGraphParamsData::GetSocketsData(const int32 PointIndex, TArray<PCGExGraph::FSocketMetadata>& OutMetadata) const
{
	OutMetadata.Reset(SocketMapping->NumSockets);
	for (const PCGExGraph::FSocket& Socket : SocketMapping->Sockets) { OutMetadata.Add(Socket.GetData(PointIndex)); }
}

void UPCGExGraphParamsData::SetSocketsData(const int32 PointIndex, TArray<PCGExGraph::FSocketMetadata>& InMetadata) const
{
	check(InMetadata.Num() == SocketMapping->NumSockets)
	for (int i = 0; i < SocketMapping->NumSockets; i++)
	{
		PCGExGraph::FSocket& Socket = SocketMapping->Sockets[i];
		Socket.SetData(PointIndex, InMetadata[i]);
	}
}

void UPCGExGraphParamsData::GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos) const
{
	SocketMapping->GetSocketsInfos(OutInfos);
}

void UPCGExGraphParamsData::Cleanup() const
{
	if (SocketMapping) { SocketMapping->Cleanup(); }
}

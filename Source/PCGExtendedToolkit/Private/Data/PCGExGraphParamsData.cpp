// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGraphParamsData.h"
#include "Data/PCGPointData.h"

struct FPCGExGraphProcessorContext;

UPCGExGraphParamsData::UPCGExGraphParamsData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExGraphParamsData::HasMatchingGraphData(const UPCGPointData* PointData)
{
	// Whether the data has metadata matching this GraphData block or not
	for (const PCGExGraph::FSocket Socket : SocketMapping.Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.GetName())) { return false; }
	}
	return true;
}

void UPCGExGraphParamsData::Initialize(
	TArray<FPCGExSocketDescriptor>& InSockets,
	const bool bApplyOverrides,
	FPCGExSocketGlobalOverrides& Overrides)
{
	SocketMapping = PCGExGraph::FSocketMapping{};

	if (bApplyOverrides) { SocketMapping.InitializeWithOverrides(GraphIdentifier, InSockets, Overrides); }
	else { SocketMapping.Initialize(GraphIdentifier, InSockets); }

	GreatestStaticMaxDistance = 0.0;
	bHasVariableMaxDistance = false;

	for (const FPCGExSocketDescriptor& Socket : InSockets)
	{
		if (!Socket.bEnabled) { continue; }
		if (Socket.bApplyAttributeModifier) { bHasVariableMaxDistance = true; }
		GreatestStaticMaxDistance = FMath::Max(GreatestStaticMaxDistance, Socket.Angle.MaxDistance);
	}

	CachedIndexAttributeName = SocketMapping.GetCompoundName(FName("CachedIndex"));
}

void UPCGExGraphParamsData::PrepareForPointData(const UPCGPointData* PointData, const bool bEnsureEdgeType)
{
	SocketMapping.PrepareForPointData(PointData, bEnsureEdgeType);
}

void UPCGExGraphParamsData::GetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExGraph::FSocketMetadata>& OutMetadata) const
{
	OutMetadata.Reset(SocketMapping.NumSockets);
	for (const PCGExGraph::FSocket& Socket : SocketMapping.Sockets) { OutMetadata.Add(Socket.GetData(MetadataEntry)); }
}

template <typename T>
void UPCGExGraphParamsData::GetEdges(const int32 InIndex, const PCGMetadataEntryKey MetadataEntry, TArray<T>& OutEdges) const
{
	for (const PCGExGraph::FSocket& Socket : SocketMapping.Sockets)
	{
		T Edge;
		if (Socket.TryGetEdge(InIndex, MetadataEntry, Edge)) { OutEdges.AddUnique(Edge); }
	}
}

void UPCGExGraphParamsData::SetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExGraph::FSocketMetadata>& InMetadata)
{
	check(InMetadata.Num() == SocketMapping.NumSockets)
	for (int i = 0; i < SocketMapping.NumSockets; i++)
	{
		PCGExGraph::FSocket& Socket = SocketMapping.Sockets[i];
		Socket.SetData(MetadataEntry, InMetadata[i]);
	}
}

void UPCGExGraphParamsData::GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos)
{
	SocketMapping.GetSocketsInfos(OutInfos);
}

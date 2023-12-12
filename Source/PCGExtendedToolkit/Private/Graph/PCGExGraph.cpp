// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

PCGExGraph::FSocket::~FSocket()
{
	MatchingSockets.Empty();
	Cleanup();
}

void PCGExGraph::FSocket::Cleanup()
{
	PCGEX_DELETE(TargetIndexWriter)
	PCGEX_DELETE(EdgeTypeWriter)
	PCGEX_DELETE(TargetIndexReader)
	PCGEX_DELETE(EdgeTypeReader)
}

void PCGExGraph::FSocket::DeleteFrom(const UPCGPointData* PointData) const
{
	if (TargetIndexReader) { PointData->Metadata->DeleteAttribute(TargetIndexReader->Name); }
	if (TargetIndexWriter) { PointData->Metadata->DeleteAttribute(TargetIndexWriter->Name); }

	if (EdgeTypeReader) { PointData->Metadata->DeleteAttribute(EdgeTypeReader->Name); }
	if (EdgeTypeWriter) { PointData->Metadata->DeleteAttribute(EdgeTypeWriter->Name); }
}

void PCGExGraph::FSocket::Write(bool DoCleanup)
{
	if (TargetIndexWriter) { TargetIndexWriter->Write(); }
	if (EdgeTypeWriter) { EdgeTypeWriter->Write(); }
	if (DoCleanup) { Cleanup(); }
}

void PCGExGraph::FSocket::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool ReadOnly)
{
	Cleanup();

	bReadOnly = ReadOnly;

	const FName NAME_Index = GetSocketPropertyName(SocketPropertyNameIndex);
	const FName NAME_EdgeType = GetSocketPropertyName(SocketPropertyNameEdgeType);

	PCGExData::FPointIO& MutablePointIO = const_cast<PCGExData::FPointIO&>(PointIO);

	if (bReadOnly)
	{
		TargetIndexReader = new PCGEx::TFAttributeReader<int32>(NAME_Index);
		EdgeTypeReader = new PCGEx::TFAttributeReader<int32>(NAME_EdgeType);
		TargetIndexReader->Bind(MutablePointIO);
		EdgeTypeReader->Bind(MutablePointIO);
	}
	else
	{
		TargetIndexWriter = new PCGEx::TFAttributeWriter<int32>(NAME_Index, -1, false);
		EdgeTypeWriter = new PCGEx::TFAttributeWriter<int32>(NAME_EdgeType, static_cast<int32>(EPCGExEdgeType::Unknown), false);
		TargetIndexWriter->BindAndGet(MutablePointIO);
		EdgeTypeWriter->BindAndGet(MutablePointIO);
	}

	Descriptor.Bounds.LoadCurve();
}

void PCGExGraph::FSocket::SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const
{
	SetTargetIndex(MetadataEntry, SocketMetadata.Index);
	SetEdgeType(MetadataEntry, SocketMetadata.EdgeType);
}

void PCGExGraph::FSocket::SetTargetIndex(const int32 PointIndex, int32 InValue) const
{
	check(!bReadOnly)
	(*TargetIndexWriter)[PointIndex] = InValue;
}

int32 PCGExGraph::FSocket::GetTargetIndex(const int32 PointIndex) const
{
	if (bReadOnly) { return (*TargetIndexReader)[PointIndex]; }
	return (*TargetIndexWriter)[PointIndex];
}

void PCGExGraph::FSocket::SetEdgeType(const int32 PointIndex, EPCGExEdgeType InEdgeType) const
{
	check(!bReadOnly)
	(*EdgeTypeWriter)[PointIndex] = static_cast<int32>(InEdgeType);
}

EPCGExEdgeType PCGExGraph::FSocket::GetEdgeType(const int32 PointIndex) const
{
	if (bReadOnly) { return static_cast<EPCGExEdgeType>((*EdgeTypeReader)[PointIndex]); }
	return static_cast<EPCGExEdgeType>((*EdgeTypeWriter)[PointIndex]);
}

PCGExGraph::FSocketMetadata PCGExGraph::FSocket::GetData(const int32 PointIndex) const
{
	return FSocketMetadata(GetTargetIndex(PointIndex), GetEdgeType(PointIndex));
}

FName PCGExGraph::FSocket::GetSocketPropertyName(FName PropertyName) const
{
	const FString Separator = TEXT("/");
	return *(AttributeNameBase.ToString() + Separator + PropertyName.ToString());
}

void PCGExGraph::FSocketMapping::Initialize(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets)
{
	Reset();
	Identifier = InIdentifier;
	for (FPCGExSocketDescriptor& Descriptor : InSockets)
	{
		if (!Descriptor.bEnabled) { continue; }

		FProbeDistanceModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
		FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);

		FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
		NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
		NewSocket.SocketIndex = NumSockets;
		NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);
		NumSockets++;
	}

	PostProcessSockets();
}

void PCGExGraph::FSocketMapping::InitializeWithOverrides(FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides)
{
	Reset();
	Identifier = InIdentifier;
	const FString PCGExName = TEXT("PCGEx");
	for (FPCGExSocketDescriptor& Descriptor : InSockets)
	{
		if (!Descriptor.bEnabled) { continue; }

		FProbeDistanceModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
		NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
		NewModifier.Descriptor = static_cast<FPCGExInputDescriptor>(Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier);

		FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
		NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
		NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

		FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
		NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
		NewSocket.SocketIndex = NumSockets;
		NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

		if (Overrides.bOverrideRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = Overrides.bRelativeOrientation; }
		if (Overrides.bOverrideAngle) { NewSocket.Descriptor.Bounds.Angle = Overrides.Angle; }
		if (Overrides.bOverrideMaxDistance) { NewSocket.Descriptor.Bounds.MaxDistance = Overrides.MaxDistance; }
		if (Overrides.bOverrideExclusiveBehavior) { NewSocket.Descriptor.bExclusiveBehavior = Overrides.bExclusiveBehavior; }
		if (Overrides.bOverrideDotOverDistance) { NewSocket.Descriptor.Bounds.DotOverDistance = Overrides.DotOverDistance; }
		if (Overrides.bOverrideOffsetOrigin) { NewSocket.Descriptor.OffsetOrigin = Overrides.OffsetOrigin; }

		NewSocket.Descriptor.Bounds.DotThreshold = FMath::Cos(NewSocket.Descriptor.Bounds.Angle * (PI / 180.0));

		NumSockets++;
	}

	PostProcessSockets();
}

FName PCGExGraph::FSocketMapping::GetCompoundName(FName SecondaryIdentifier) const
{
	const FString Separator = TEXT("/");
	return *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + SecondaryIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
}

void PCGExGraph::FSocketMapping::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly)
{
	for (int i = 0; i < Sockets.Num(); i++)
	{
		Sockets[i].PrepareForPointData(PointIO, bReadOnly);
		Modifiers[i].Bind(PointIO);
		LocalDirections[i].Bind(PointIO);
	}
}

void PCGExGraph::FSocketMapping::GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
{
	OutInfos.Empty(NumSockets);
	for (int i = 0; i < NumSockets; i++)
	{
		FSocketInfos& Infos = OutInfos.Emplace_GetRef();
		Infos.Socket = &(Sockets[i]);
		Infos.Modifier = &(Modifiers[i]);
		Infos.LocalDirection = &(LocalDirections[i]);
	}
}

void PCGExGraph::FSocketMapping::Cleanup()
{
	for (FSocket& Socket : Sockets) { Socket.Cleanup(); }
	for (FProbeDistanceModifier& Modifier : Modifiers) { Modifier.Cleanup(); }
	for (FLocalDirection& Direction : LocalDirections) { Direction.Cleanup(); }
}

void PCGExGraph::FSocketMapping::Reset()
{
	Sockets.Empty();
	Modifiers.Empty();
	LocalDirections.Empty();
}

void PCGExGraph::FSocketMapping::PostProcessSockets()
{
	for (FSocket& Socket : Sockets)
	{
		for (const FName MatchingSocketName : Socket.Descriptor.MatchingSlots)
		{
			FName OtherSocketName = GetCompoundName(MatchingSocketName);
			if (const int32* Index = NameToIndexMap.Find(OtherSocketName))
			{
				Socket.MatchingSockets.Add(*Index);
				if (Socket.Descriptor.bMirrorMatchingSockets) { Sockets[*Index].MatchingSockets.Add(Socket.SocketIndex); }
			}
		}
	}
}

EPCGExEdgeType PCGExGraph::GetEdgeType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket)
{
	if (StartSocket.Matches(EndSocket))
	{
		if (EndSocket.Matches(StartSocket))
		{
			return EPCGExEdgeType::Complete;
		}
		return EPCGExEdgeType::Match;
	}
	if (StartSocket.Socket->SocketIndex == EndSocket.Socket->SocketIndex)
	{
		// We check for mirror AFTER checking for shared/match, since Mirror can be considered a legal match by design
		// in which case we don't want to flag this as Mirrored.
		return EPCGExEdgeType::Mirror;
	}
	return EPCGExEdgeType::Shared;
}

void PCGExGraph::ComputeEdgeType(const TArray<FSocketInfos>& SocketInfos, const int32 PointIndex)
{
	for (const FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
		const int64 RelationIndex = CurrentSocketInfos.Socket->GetTargetIndex(PointIndex);

		if (RelationIndex != -1)
		{
			for (const FSocketInfos& OtherSocketInfos : SocketInfos)
			{
				if (OtherSocketInfos.Socket->GetTargetIndex(RelationIndex) == PointIndex)
				{
					Type = GetEdgeType(CurrentSocketInfos, OtherSocketInfos);
				}
			}

			if (Type == EPCGExEdgeType::Unknown) { Type = EPCGExEdgeType::Roaming; }
		}


		CurrentSocketInfos.Socket->SetEdgeType(PointIndex, Type);
	}
}

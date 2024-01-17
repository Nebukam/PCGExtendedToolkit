// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGraphParamsData.h"
#include "Data/PCGPointData.h"

struct PCGEXTENDEDTOOLKIT_API FPCGExCustomGraphProcessorContext;

namespace PCGExGraph
{
	FSocket::~FSocket()
	{
		MatchingSockets.Empty();
		Cleanup();
	}

	void FSocket::Cleanup()
	{
		PCGEX_DELETE(TargetIndexWriter)
		PCGEX_DELETE(EdgeTypeWriter)
		PCGEX_DELETE(TargetIndexReader)
		PCGEX_DELETE(EdgeTypeReader)
	}

	void FSocket::DeleteFrom(const UPCGPointData* PointData) const
	{
		const FName NAME_Index = GetSocketPropertyName(SocketPropertyNameIndex);
		const FName NAME_EdgeType = GetSocketPropertyName(SocketPropertyNameEdgeType);

		if (PointData->Metadata->HasAttribute(NAME_Index)) { PointData->Metadata->DeleteAttribute(NAME_Index); }
		if (PointData->Metadata->HasAttribute(NAME_EdgeType)) { PointData->Metadata->DeleteAttribute(NAME_EdgeType); }
	}

	void FSocket::Write(const bool bDoCleanup)
	{
		if (TargetIndexWriter) { TargetIndexWriter->Write(); }
		if (EdgeTypeWriter) { EdgeTypeWriter->Write(); }
		if (bDoCleanup) { Cleanup(); }
	}

	void FSocket::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool ReadOnly)
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

	void FSocket::SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const
	{
		SetTargetIndex(MetadataEntry, SocketMetadata.Index);
		SetEdgeType(MetadataEntry, SocketMetadata.EdgeType);
	}

	void FSocket::SetTargetIndex(const int32 PointIndex, const int32 InValue) const
	{
		check(!bReadOnly)
		(*TargetIndexWriter)[PointIndex] = InValue;
	}

	int32 FSocket::GetTargetIndex(const int32 PointIndex) const
	{
		if (bReadOnly) { return (*TargetIndexReader)[PointIndex]; }
		return (*TargetIndexWriter)[PointIndex];
	}

	void FSocket::SetEdgeType(const int32 PointIndex, EPCGExEdgeType InEdgeType) const
	{
		check(!bReadOnly)
		(*EdgeTypeWriter)[PointIndex] = static_cast<int32>(InEdgeType);
	}

	EPCGExEdgeType FSocket::GetEdgeType(const int32 PointIndex) const
	{
		if (bReadOnly) { return static_cast<EPCGExEdgeType>((*EdgeTypeReader)[PointIndex]); }
		return static_cast<EPCGExEdgeType>((*EdgeTypeWriter)[PointIndex]);
	}

	FSocketMetadata FSocket::GetData(const int32 PointIndex) const
	{
		return FSocketMetadata(GetTargetIndex(PointIndex), GetEdgeType(PointIndex));
	}

	FName FSocket::GetSocketPropertyName(const FName PropertyName) const
	{
		const FString Separator = TEXT("/");
		return *(AttributeNameBase.ToString() + Separator + PropertyName.ToString());
	}

	void FSocketMapping::Initialize(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets)
	{
		Reset();
		Identifier = InIdentifier;
		for (FPCGExSocketDescriptor& Descriptor : InSockets)
		{
			if (!Descriptor.bEnabled) { continue; }

			MaxDistanceGetters.Emplace_GetRef(Descriptor);
			LocalDirectionGetters.Emplace_GetRef(Descriptor);

			FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
			NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;
			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);
		}

		PostProcessSockets();
	}

	void FSocketMapping::InitializeWithOverrides(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides)
	{
		Reset();
		Identifier = InIdentifier;
		const FString PCGExName = TEXT("PCGEx");
		for (FPCGExSocketDescriptor& Descriptor : InSockets)
		{
			if (!Descriptor.bEnabled) { continue; }

			FProbeDistanceModifier& NewModifier = MaxDistanceGetters.Emplace_GetRef(Descriptor);
			NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
			NewModifier.Descriptor = static_cast<FPCGExInputDescriptor>(Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier);

			FLocalDirection& NewLocalDirection = LocalDirectionGetters.Emplace_GetRef(Descriptor);
			NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
			NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

			FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
			NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;
			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

			if (Overrides.bOverrideRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = Overrides.bRelativeOrientation; }
			if (Overrides.bOverrideAngle) { NewSocket.Descriptor.Bounds.Angle = Overrides.Angle; }
			if (Overrides.bOverrideMaxDistance) { NewSocket.Descriptor.Bounds.MaxDistance = Overrides.MaxDistance; }
			if (Overrides.bOverrideExclusiveBehavior) { NewSocket.Descriptor.bExclusiveBehavior = Overrides.bExclusiveBehavior; }
			if (Overrides.bOverrideDotOverDistance) { NewSocket.Descriptor.Bounds.DotOverDistance = Overrides.DotOverDistance; }
			if (Overrides.bOverrideOffsetOrigin) { NewSocket.Descriptor.OffsetOrigin = Overrides.OffsetOrigin; }

			NewSocket.Descriptor.Bounds.DotThreshold = FMath::Cos(NewSocket.Descriptor.Bounds.Angle * (PI / 180.0));
		}

		PostProcessSockets();
	}

	FName FSocketMapping::GetCompoundName(const FName SecondaryIdentifier) const
	{
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + SecondaryIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
	}

	void FSocketMapping::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly)
	{

		//TODO: Write index per graph instead of per socket
		//GetRemappedIndices(PointIO, ) GetParamPropertyName(ParamPropertyNameIndex)
		
		
		for (int i = 0; i < Sockets.Num(); i++)
		{
			Sockets[i].PrepareForPointData(PointIO, bReadOnly);
			MaxDistanceGetters[i].Grab(PointIO);
			LocalDirectionGetters[i].Grab(PointIO);
		}
	}

	void FSocketMapping::GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
	{
		OutInfos.Empty(NumSockets);
		for (int i = 0; i < NumSockets; i++)
		{
			FSocketInfos& Infos = OutInfos.Emplace_GetRef();
			Infos.Socket = &(Sockets[i]);
			Infos.MaxDistanceGetter = &(MaxDistanceGetters[i]);
			Infos.LocalDirectionGetter = &(LocalDirectionGetters[i]);
		}
	}

	void FSocketMapping::Cleanup()
	{
		for (FSocket& Socket : Sockets) { Socket.Cleanup(); }
		for (FProbeDistanceModifier& Modifier : MaxDistanceGetters) { Modifier.Cleanup(); }
		for (FLocalDirection& Direction : LocalDirectionGetters) { Direction.Cleanup(); }
	}

	void FSocketMapping::Reset()
	{
		Sockets.Empty();
		MaxDistanceGetters.Empty();
		LocalDirectionGetters.Empty();
	}

	FName FSocketMapping::GetParamPropertyName(const FName PropertyName) const
	{
		const FString Separator = TEXT("/");
		return *(Identifier.ToString() + Separator + PropertyName.ToString());
	}

	void FSocketMapping::PostProcessSockets()
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
}

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

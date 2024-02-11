// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGraphDefinition.h"
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
		PCGEX_DELETE(LocalDirectionGetter)
		PCGEX_DELETE(LocalAngleGetter)
		PCGEX_DELETE(LocalRadiusGetter)

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

#define PCGEX_LOCAL_SOCKET_ATT(_NAME, _TYPE)\
		if(Descriptor.bUseLocal##_NAME)\
		{\
			if(!Local##_NAME##Getter){Local##_NAME##Getter = new PCGEx::FLocal##_TYPE##Getter();}\
			Local##_NAME##Getter->Capture(Descriptor.Local##_NAME);\
			Local##_NAME##Getter->Grab(PointIO, true);	\
		}else{ PCGEX_DELETE(Local##_NAME##Getter) }

		PCGEX_LOCAL_SOCKET_ATT(Direction, Vector)
		PCGEX_LOCAL_SOCKET_ATT(Angle, SingleField)
		PCGEX_LOCAL_SOCKET_ATT(Radius, SingleField)

		if (LocalAngleGetter && LocalAngleGetter->IsUsable(PointIO.GetNum()) && Descriptor.bLocalAngleIsDegrees)
		{
			for (int i = 0; i < LocalAngleGetter->Values.Num(); i++) { LocalAngleGetter->Values[i] = PCGExMath::DegreesToDot(LocalAngleGetter->Values[i]); }
		}

#undef PCGEX_LOCAL_SOCKET_ATT

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

		Descriptor.LoadCurve();
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

	void FSocketMapping::Initialize(
		const FName InIdentifier,
		TArray<FPCGExSocketDescriptor>& InSockets,
		const FPCGExSocketGlobalOverrides& Overrides,
		const FPCGExSocketDescriptor& OverrideSocket)
	{
		Reset();
		Identifier = InIdentifier;
		const FString PCGExName = TEXT("PCGEx");
		const bool bDoOverride = Overrides.bEnabled && !OverrideSocket.SocketName.IsNone();
		for (FPCGExSocketDescriptor& Descriptor : InSockets)
		{
			if (!Descriptor.bEnabled) { continue; }

			FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
			NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;

			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

			if (bDoOverride)
			{
				//

				if (Overrides.bRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = OverrideSocket.bRelativeOrientation; }

				if (Overrides.bDirection) { NewSocket.Descriptor.Direction = OverrideSocket.Direction; }
				if (Overrides.bUseLocalDirection) { NewSocket.Descriptor.bUseLocalDirection = OverrideSocket.bUseLocalDirection; }
				if (Overrides.bLocalDirection) { NewSocket.Descriptor.LocalDirection = OverrideSocket.LocalDirection; }

				//

				if (Overrides.bAngle) { NewSocket.Descriptor.Angle = OverrideSocket.Angle; }
				if (Overrides.bUseLocalAngle) { NewSocket.Descriptor.bUseLocalAngle = OverrideSocket.bUseLocalAngle; }
				if (Overrides.bLocalAngle) { NewSocket.Descriptor.LocalAngle = OverrideSocket.LocalAngle; }
				if (Overrides.bLocalAngleIsDegrees) { NewSocket.Descriptor.bLocalAngleIsDegrees = OverrideSocket.bLocalAngleIsDegrees; }

				//

				if (Overrides.bRadius) { NewSocket.Descriptor.Radius = OverrideSocket.Radius; }
				if (Overrides.bUseLocalRadius) { NewSocket.Descriptor.bUseLocalRadius = OverrideSocket.bUseLocalRadius; }
				if (Overrides.bLocalRadius) { NewSocket.Descriptor.LocalRadius = OverrideSocket.LocalRadius; }

				if (Overrides.bDotOverDistance) { NewSocket.Descriptor.DotOverDistance = OverrideSocket.DotOverDistance; }
				if (Overrides.bDistanceSettings) { NewSocket.Descriptor.DistanceSettings = OverrideSocket.DistanceSettings; }

				if (Overrides.bMirrorMatchingSockets) { NewSocket.Descriptor.bMirrorMatchingSockets = OverrideSocket.bMirrorMatchingSockets; }
			}

			NewSocket.Descriptor.DotThreshold = PCGExMath::DegreesToDot(NewSocket.Descriptor.Angle);
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

		for (int i = 0; i < Sockets.Num(); i++) { Sockets[i].PrepareForPointData(PointIO, bReadOnly); }
	}

	void FSocketMapping::GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
	{
		OutInfos.Empty(NumSockets);
		for (int i = 0; i < NumSockets; i++) { OutInfos.Emplace_GetRef(&(Sockets[i])); }
	}

	void FSocketMapping::Cleanup()
	{
		for (FSocket& Socket : Sockets) { Socket.Cleanup(); }
	}

	void FSocketMapping::Reset()
	{
		Sockets.Empty();
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

UPCGExGraphDefinition::UPCGExGraphDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExGraphDefinition::HasMatchingGraphDefinition(const UPCGPointData* PointData) const
{
	// Whether the data has metadata matching this GraphData block or not
	for (const PCGExGraph::FSocket Socket : SocketMapping->Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.GetName())) { return false; }
	}
	return true;
}

void UPCGExGraphDefinition::BeginDestroy()
{
	Cleanup();

	PCGEX_DELETE(SocketMapping);
	SocketsDescriptors.Empty();
	Super::BeginDestroy();
}

void UPCGExGraphDefinition::Initialize()
{
	PCGEX_DELETE(SocketMapping);

	SocketMapping = new PCGExGraph::FSocketMapping();
	SocketMapping->Initialize(
		GraphIdentifier,
		SocketsDescriptors,
		GlobalOverrides,
		OverrideSocket);

	CachedIndexAttributeName = SocketMapping->GetCompoundName(FName("CachedIndex"));
}

void UPCGExGraphDefinition::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly = true) const
{
	SocketMapping->PrepareForPointData(PointIO, bReadOnly);
}

void UPCGExGraphDefinition::GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos) const
{
	SocketMapping->GetSocketsInfos(OutInfos);
}

void UPCGExGraphDefinition::Cleanup() const
{
	if (SocketMapping) { SocketMapping->Cleanup(); }
}

UPCGExSocketDefinition::UPCGExSocketDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPCGExSocketDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

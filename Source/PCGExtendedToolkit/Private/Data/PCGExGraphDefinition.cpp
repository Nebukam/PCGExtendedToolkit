// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGraphDefinition.h"
#include "Data/PCGPointData.h"

struct PCGEXTENDEDTOOLKIT_API FPCGExCustomGraphProcessorContext;

namespace PCGExGraph
{
#pragma region FSocket

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

#pragma endregion

#pragma region FSocketMapping

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
			NewSocket.AttributeNameBase = PCGEx::GetCompoundName(Identifier, Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;

			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

			if (bDoOverride)
			{
				//

				if (Overrides.bRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = OverrideSocket.bRelativeOrientation; }

				if (Overrides.bUseLocalDirection) { NewSocket.Descriptor.bUseLocalDirection = OverrideSocket.bUseLocalDirection; }

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

			NewSocket.Descriptor.Direction.Normalize();
			NewSocket.Descriptor.DotThreshold = PCGExMath::DegreesToDot(NewSocket.Descriptor.Angle);
		}

		PostProcessSockets();
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
				FName OtherSocketName = PCGEx::GetCompoundName(Identifier, MatchingSocketName);
				if (const int32* Index = NameToIndexMap.Find(OtherSocketName))
				{
					Socket.MatchingSockets.Add(*Index);
					if (Socket.Descriptor.bMirrorMatchingSockets) { Sockets[*Index].MatchingSockets.Add(Socket.SocketIndex); }
				}
			}
		}
	}

#pragma endregion
}


#pragma region UPCGExGraphDefinition

UPCGExGraphDefinition::UPCGExGraphDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExGraphDefinition::HasMatchingGraphDefinition(const UPCGPointData* PointData) const
{
	// Whether the data has metadata matching this GraphData block or not
	for (const PCGExGraph::FSocket& Socket : SocketMapping->Sockets)
	{
		if (!PointData->Metadata->HasAttribute(Socket.GetName())) { return false; }
	}
	return true;
}

bool UPCGExGraphDefinition::ContainsNamedSocked(const FName InName) const
{
	for (const FPCGExSocketDescriptor& Socket : SocketsDescriptors) { if (Socket.SocketName == InName) { return true; } }
	return false;
}

void UPCGExGraphDefinition::AddSocketNames(TSet<FName>& OutUniqueNames) const
{
	for (const FPCGExSocketDescriptor& Socket : SocketsDescriptors) { OutUniqueNames.Add(Socket.SocketName); }
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

	CachedIndexAttributeName = PCGEx::GetCompoundName(GraphIdentifier, FName("CachedIndex"));
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

#pragma endregion

PCGExFactories::EType UPCGExSocketStateFactory::GetFactoryType() const
{
	return PCGExFactories::EType::SocketState;
}

PCGExDataFilter::TFilter* UPCGExSocketStateFactory::CreateFilter() const
{
	return new PCGExGraph::FSocketStateHandler(this);
}

void UPCGExSocketStateFactory::BeginDestroy()
{
	FilterFactories.Empty();
	Super::BeginDestroy();
}

namespace PCGExGraph
{
#pragma region FSocketStateHandler

	FSocketStateHandler::FSocketStateHandler(const UPCGExSocketStateFactory* InDefinition)
		: TDataState(InDefinition), SocketStateDefinition(InDefinition)
	{
		const int32 NumTests = InDefinition->FilterFactories.Num();
		EdgeTypeAttributes.SetNumUninitialized(NumTests);
		EdgeTypeReaders.SetNumUninitialized(NumTests);
		for (int i = 0; i < NumTests; i++)
		{
			EdgeTypeAttributes[i] = nullptr;
			EdgeTypeReaders[i] = nullptr;
		}
	}

	void FSocketStateHandler::CaptureGraph(const FGraphInputs* GraphInputs, const PCGExData::FPointIO* InPointIO)
	{
		for (const UPCGExGraphDefinition* Graph : GraphInputs->Params) { CaptureGraph(Graph, InPointIO); }
	}

	void FSocketStateHandler::CaptureGraph(const UPCGExGraphDefinition* Graph, const PCGExData::FPointIO* InPointIO)
	{
		const int32 NumConditions = SocketStateDefinition->FilterFactories.Num();
		int32 NumEnabledConditions = 0;
		UPCGMetadata* Metadata = InPointIO->GetIn()->Metadata;
		for (int i = 0; i < NumConditions; i++)
		{
			if (EdgeTypeAttributes[i]) { continue; } // Spot already taken by another graph

			const FPCGExSocketTestDescriptor& Condition = SocketStateDefinition->FilterFactories[i];
			if (!Condition.bEnabled) { continue; }

			NumEnabledConditions++;

			const FName SocketEdgeTypeName = PCGEx::GetCompoundName(Graph->GraphIdentifier, Condition.SocketName, SocketPropertyNameEdgeType);
			if (FPCGMetadataAttributeBase* AttBase = Metadata->GetMutableAttribute(SocketEdgeTypeName);
				AttBase && AttBase->GetTypeId() == static_cast<int16>(EPCGMetadataTypes::Integer32))
			{
				FPCGMetadataAttribute<int32>* TypedAtt = static_cast<FPCGMetadataAttribute<int32>*>(AttBase);
				EdgeTypeAttributes[i] = TypedAtt;
			}
		}

		int32 NumValid = 0;
		for (int i = 0; i < EdgeTypeAttributes.Num(); i++) { if (EdgeTypeAttributes[i]) { NumValid++; } }

		bValid = NumValid > 0;
		bPartial = (NumEnabledConditions != NumValid);
	}

	bool FSocketStateHandler::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TDataState::PrepareForTesting(PointIO);

		for (int i = 0; i < EdgeTypeAttributes.Num(); i++)
		{
			if (!EdgeTypeAttributes[i]) { continue; }
			PCGEx::TFAttributeReader<int32>* Reader = new PCGEx::TFAttributeReader<int32>(EdgeTypeAttributes[i]->Name);
			EdgeTypeReaders[i] = Reader;
			Reader->Bind(const_cast<PCGExData::FPointIO&>(*PointIO));
		}

		return false;
	}

	bool FSocketStateHandler::Test(const int32 PointIndex) const
	{
		for (int i = 0; i < SocketStateDefinition->FilterFactories.Num(); i++)
		{
			PCGEx::TFAttributeReader<int32>* Reader = EdgeTypeReaders[i];
			if (!Reader) { continue; }
			if (!SocketStateDefinition->FilterFactories[i].MeetCondition(Reader->Values[PointIndex])) { return false; }
		}

		return true;
	}

#pragma endregion
}

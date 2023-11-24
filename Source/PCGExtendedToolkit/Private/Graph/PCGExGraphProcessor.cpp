// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraphProcessor.h"

#include "PCGPin.h"

#include "Data/PCGExGraphParamsData.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


TArray<FPCGPinProperties> UPCGExGraphProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExGraph::SourceParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertyParams.Tooltip = LOCTEXT("PCGExSourceParamsPinTooltip", "Graph Params. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExGraphProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinParamsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinParamsOutput.Tooltip = LOCTEXT("PCGExOutputParamsTooltip", "Graph Params forwarding. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

/*
bool UPCGExGraphProcessorSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
#if WITH_EDITOR
	if (InPin->Properties.Label == PCGExGraph::SourceParamsLabel)
	{
		auto PCGExPlugin = IPluginManager::Get().FindPlugin(TEXT("PCGExtendedToolkit"));
		if (PCGExPlugin.IsValid())
		{
			OutExtraIcon = FName(PCGExPlugin->GetBaseDir() + "/Content/Editor/Slate/Icons/PCGExGraphParams.png");
			return true;
		}
	}
#endif // WITH_EDITOR
	return false;
}
*/

#pragma endregion

bool FPCGExGraphProcessorContext::AdvanceGraph(bool bResetPointsIndex)
{
	if (bResetPointsIndex) { CurrentPointsIndex = -1; }
	CurrentParamsIndex++;
	if (Params.Params.IsValidIndex(CurrentParamsIndex))
	{
		CurrentGraph = Params.Params[CurrentParamsIndex];
		CurrentGraph->GetSocketsInfos(SocketInfos);
		return true;
	}

	CurrentGraph = nullptr;
	return false;
}

bool FPCGExGraphProcessorContext::AdvancePointsIO(bool bResetParamsIndex)
{
	if (bResetParamsIndex) { CurrentParamsIndex = -1; }
	return FPCGExPointsProcessorContext::AdvancePointsIO();
}

void FPCGExGraphProcessorContext::Reset()
{
	FPCGExPointsProcessorContext::Reset();
	CurrentParamsIndex = -1;
}

void FPCGExGraphProcessorContext::ComputeEdgeType(const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* PointIO)
{
	for (PCGExGraph::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
		const int64 RelationIndex = CurrentSocketInfos.Socket->GetTargetIndex(Point.MetadataEntry);
		
		if (RelationIndex != -1)
		{
			const int32 Key = PointIO->Out->GetPoint(RelationIndex).MetadataEntry;
			for (PCGExGraph::FSocketInfos& OtherSocketInfos : SocketInfos)
			{
				if (OtherSocketInfos.Socket->GetTargetIndex(Key) == ReadIndex)
				{
					Type = PCGExGraph::Helpers::GetEdgeType(CurrentSocketInfos, OtherSocketInfos);
				}
			}

			if (Type == EPCGExEdgeType::Unknown) { Type = EPCGExEdgeType::Roaming; }
		}


		CurrentSocketInfos.Socket->SetEdgeType(Point.MetadataEntry, Type);
	}
}

double FPCGExGraphProcessorContext::PrepareProbesForPoint(const FPCGPoint& Point, TArray<PCGExGraph::FSocketProbe>& OutProbes)
{
	OutProbes.Reset(SocketInfos.Num());
	double MaxDistance = 0.0;
	for (PCGExGraph::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		PCGExGraph::FSocketProbe& NewProbe = OutProbes.Emplace_GetRef();
		NewProbe.SocketInfos = &CurrentSocketInfos;
		PrepareProbeForPointSocketPair(Point, NewProbe, CurrentSocketInfos);
		MaxDistance = FMath::Max(MaxDistance, NewProbe.MaxDistance);
	}
	return MaxDistance;
}

void FPCGExGraphProcessorContext::PrepareCurrentGraphForPoints(const UPCGPointData* InData, bool bEnsureEdgeType)
{
	CachedIndex = InData->Metadata->FindOrCreateAttribute<int64>(CurrentGraph->CachedIndexAttributeName, -1, false);
	CurrentGraph->PrepareForPointData(InData, bEnsureEdgeType);
}

void FPCGExGraphProcessorContext::PrepareProbeForPointSocketPair(
	const FPCGPoint& Point,
	PCGExGraph::FSocketProbe& Probe,
	PCGExGraph::FSocketInfos InSocketInfos)
{
	const FPCGExSocketAngle& BaseAngle = InSocketInfos.Socket->Descriptor.Angle;

	FVector Direction = BaseAngle.Direction;
	double DotTolerance = BaseAngle.DotThreshold;
	double MaxDistance = BaseAngle.MaxDistance;

	const FTransform PtTransform = Point.Transform;
	Probe.Origin = PtTransform.GetLocation();

	if (InSocketInfos.Socket->Descriptor.bRelativeOrientation)
	{
		Direction = PtTransform.Rotator().RotateVector(Direction);
		Direction.Normalize();
	}

	if (InSocketInfos.Modifier &&
		InSocketInfos.Modifier->bEnabled &&
		InSocketInfos.Modifier->bValid)
	{
		MaxDistance *= InSocketInfos.Modifier->GetValue(Point);
	}

	if (InSocketInfos.LocalDirection &&
		InSocketInfos.LocalDirection->bEnabled &&
		InSocketInfos.LocalDirection->bValid)
	{
		// TODO: Apply LocalDirection
	}

	Probe.Direction = Direction;
	Probe.DotThreshold = DotTolerance;
	Probe.MaxDistance = MaxDistance * MaxDistance;
	Probe.DotOverDistanceCurve = BaseAngle.DotOverDistanceCurve;
}

FPCGContext* FPCGExGraphProcessorElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExGraphProcessorContext* Context = new FPCGExGraphProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExGraphProcessorElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExGraphProcessorContext* Context = static_cast<FPCGExGraphProcessorContext*>(InContext);

	if (Context->Params.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
		return false;
	}

	return true;
}

void FPCGExGraphProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	FPCGExGraphProcessorContext* Context = static_cast<FPCGExGraphProcessorContext*>(InContext);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceParamsLabel);
	Context->Params.Initialize(InContext, Sources);
}


#undef LOCTEXT_NAMESPACE

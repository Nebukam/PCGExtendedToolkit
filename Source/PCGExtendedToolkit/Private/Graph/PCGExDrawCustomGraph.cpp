// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDrawCustomGraph.h"
#include "Graph/Solvers/PCGExCustomGraphSolver.h"

#define LOCTEXT_NAMESPACE "PCGExDrawCustomGraph"
#define PCGEX_NAMESPACE DrawCustomGraph

PCGExData::EInit UPCGExDrawCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

UPCGExDrawCustomGraphSettings::UPCGExDrawCustomGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	DebugSettings.PointScale = 0.0f;
#endif
}

#if WITH_EDITOR
void UPCGExDrawCustomGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DebugSettings.PointScale = 0.0f;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(DrawCustomGraph)

bool FPCGExDrawCustomGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

#if WITH_EDITOR

	PCGEX_CONTEXT_AND_SETTINGS(DrawCustomGraph)

	if (!Settings->bPCGExDebug) { return false; }

	Context->EdgeCrawlingSettings = Settings->EdgeCrawlingSettings;

	Context->GraphSolver = Context->RegisterOperation<UPCGExCustomGraphSolver>();

#endif

	return true;
}

bool FPCGExDrawCustomGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawCustomGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DrawCustomGraph)

#if WITH_EDITOR

	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			DisabledPassThroughData(Context);
			return true;
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
		return false;
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->CurrentIO->CreateInKeys();
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
		else
		{
			if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO))
			{
				PCGEX_GRAPH_MISSING_METADATA
				return false;
			}
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const PCGEx::FPointRef& Point = PointIO.GetInPointRef(PointIndex);
			const FVector Start = Point.Point->Transform.GetLocation();

			TArray<PCGExGraph::FSocketProbe> Probes;
			if ((Settings->bDrawSocketCones || Settings->bDrawSocketBox) && Context->GraphSolver) { Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes); }

			for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
			{
				const PCGExGraph::FSocketMetadata SocketMetadata = SocketInfos.Socket->GetData(PointIndex);

				if (!PointIO.GetIn()->GetPoints().IsValidIndex(SocketMetadata.Index)) { continue; } // Attempting to draw a graph with the wrong set of input points

				if (Settings->bDrawSocketCones && Context->GraphSolver)
				{
					for (PCGExGraph::FSocketProbe Probe : Probes)
					{
						const double AngleWidth = FMath::Acos(FMath::Max(-1.0, FMath::Min(1.0, Probe.DotThreshold)));
						DrawDebugCone(
							Context->World,
							Probe.Origin,
							Probe.Direction,
							FMath::Sqrt(Probe.Radius),
							AngleWidth, AngleWidth, 12,
							Probe.SocketInfos->Socket->Descriptor.DebugColor,
							true, -1, 0, .5f);
					}
				}

				if (Settings->bDrawSocketBox && Context->GraphSolver)
				{
					for (PCGExGraph::FSocketProbe Probe : Probes)
					{
						DrawDebugBox(
							Context->World,
							Probe.CompoundBounds.GetCenter(),
							Probe.CompoundBounds.GetExtent(),
							Probe.SocketInfos->Socket->Descriptor.DebugColor,
							true, -1, 0, .5f);
					}
				}

				if (Settings->bDrawCustomGraph)
				{
					if (SocketMetadata.Index == -1) { continue; }
					if (static_cast<uint8>((SocketMetadata.EdgeType & static_cast<EPCGExEdgeType>(Context->CurrentGraphEdgeCrawlingTypes))) == 0) { continue; }

					const FPCGPoint& PtB = PointIO.GetInPoint(SocketMetadata.Index);
					FVector End = PtB.Transform.GetLocation();
					float Thickness = 1.0f;
					float ArrowSize = 0.0f;
					float Lerp = 1.0f;

					switch (SocketMetadata.EdgeType)
					{
					case EPCGExEdgeType::Unknown:
						Lerp = 0.8f;
						Thickness = 0.5f;
						ArrowSize = 1.0f;
						break;
					case EPCGExEdgeType::Roaming:
						Lerp = 0.8f;
						ArrowSize = 1.0f;
						break;
					case EPCGExEdgeType::Shared:
						Lerp = 0.4f;
						ArrowSize = 2.0f;
						break;
					case EPCGExEdgeType::Match:
						Lerp = 0.5f;
						Thickness = 2.0f;
						break;
					case EPCGExEdgeType::Complete:
						Lerp = 0.5f;
						Thickness = 2.0f;
						break;
					case EPCGExEdgeType::Mirror:
						ArrowSize = 2.0f;
						Lerp = 0.5f;
						break;
					default:
						//Lerp = 0.0f;
						//Alpha = 0.0f;
						;
					}

					if (ArrowSize > 0.0f)
					{
						DrawDebugDirectionalArrow(Context->World, Start, FMath::Lerp(Start, End, Lerp), 3.0f, SocketInfos.Socket->Descriptor.DebugColor, true, -1, 0, Thickness);
					}
					else
					{
						DrawDebugLine(Context->World, Start, FMath::Lerp(Start, End, Lerp), SocketInfos.Socket->Descriptor.DebugColor, true, -1, 0, Thickness);
					}
				}
			}
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint, true)) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsDone()) { DisabledPassThroughData(Context); }

	return Context->IsDone();

#endif

	DisabledPassThroughData(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDrawGraph.h"
#include "Graph/Solvers/PCGExGraphSolver.h"

#define LOCTEXT_NAMESPACE "PCGExDrawGraph"
#define PCGEX_NAMESPACE DrawGraph

PCGExData::EInit UPCGExDrawGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

UPCGExDrawGraphSettings::UPCGExDrawGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugSettings.PointScale = 0.0f;
}

TArray<FPCGPinProperties> UPCGExDrawGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	Empty.Empty();
	return Empty;
}

#if WITH_EDITOR
void UPCGExDrawGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//if (const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World()) { FlushPersistentDebugLines(EditorWorld); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(DrawGraph)

bool FPCGExDrawGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

#if WITH_EDITOR

	PCGEX_CONTEXT_AND_SETTINGS(DrawGraph)
	
	PCGEX_DEBUG_NOTIFY

	Context->GraphSolver = Context->RegisterOperation<UPCGExGraphSolver>();
	
#endif
	
	return true;
}

bool FPCGExDrawGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawGraphElement::Execute);

#if WITH_EDITOR

	PCGEX_CONTEXT_AND_SETTINGS(DrawGraph)

	if (Context->IsSetup())
	{	
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
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
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO);
		};

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
							FMath::Sqrt(Probe.MaxDistance),
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
							Probe.LooseBounds.GetCenter(),
							Probe.LooseBounds.GetExtent(),
							Probe.SocketInfos->Socket->Descriptor.DebugColor,
							true, -1, 0, .5f);
					}
				}

				if (Settings->bDrawGraph)
				{
					if (SocketMetadata.Index == -1) { continue; }
					if (static_cast<uint8>((SocketMetadata.EdgeType & static_cast<EPCGExEdgeType>(Settings->EdgeType))) == 0) { continue; }

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

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint, true))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsDone())
	{
		//Context->OutputPointsAndParams();
	}

	return Context->IsDone();

#endif

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

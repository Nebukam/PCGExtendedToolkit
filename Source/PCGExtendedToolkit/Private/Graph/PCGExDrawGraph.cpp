// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDrawGraph.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "PCGExtendedToolkit.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExDrawGraph"

PCGEx::EIOInit UPCGExDrawGraphSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

FPCGElementPtr UPCGExDrawGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExDrawGraphElement>();
}

UPCGExDrawGraphSettings::UPCGExDrawGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugSettings.PointScale = 0.0f;
	//DebugSettings.PointMesh = FPCGExtendedToolkitModule::Get ? FPCGExtendedToolkitModule::Get->DebugMeshFrustrum : nullptr;
}

#if WITH_EDITOR
TArray<FPCGPinProperties> UPCGExDrawGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> None;
	return None;
}

void UPCGExDrawGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World()) { FlushPersistentDebugLines(EditorWorld); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExDrawGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawGraphElement::Execute);

#if  WITH_EDITOR

	FPCGExGraphProcessorContext* Context = static_cast<FPCGExGraphProcessorContext*>(InContext);

	UPCGExDrawGraphSettings* Settings = const_cast<UPCGExDrawGraphSettings*>(Context->GetInputSettings<UPCGExDrawGraphSettings>());
	check(Settings);

	//Settings->DebugSettings.PointMesh = FPCGExtendedToolkitModule::Get->DebugMeshFrustrum;
	// Get the path of the static mesh

	if (Context->IsSetup())
	{
		FlushPersistentDebugLines(Context->World);

		if (!Validate(Context)) { return true; }
		if (!Settings->bDebug) { return true; }

		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessPoint = [&Context, &Settings](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		//FWriteScopeLock ScopeLock(Context->ContextLock);

		const FVector Start = Point.Transform.GetLocation();

		TArray<PCGExGraph::FSocketProbe> Probes;
		if (Settings->bDrawSocketCones) { Context->PrepareProbesForPoint(Point, Probes); }

		for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const PCGExGraph::FSocketMetadata SocketMetadata = SocketInfos.Socket->GetData(Point.MetadataEntry);

			if (Settings->bDrawSocketCones)
			{
				for (PCGExGraph::FSocketProbe Probe : Probes)
				{
					double AngleWidth = FMath::Acos(FMath::Max(-1.0, FMath::Min(1.0, Probe.DotThreshold)));
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

			if (Settings->bDrawGraph)
			{
				if (SocketMetadata.Index == -1) { continue; }
				if (Settings->bFilterEdges && SocketMetadata.EdgeType != Settings->EdgeType) { continue; }

				FPCGPoint PtB = IO->In->GetPoint(SocketMetadata.Index);
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
				case EPCGExEdgeType::Unique:
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

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->In, false);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		//if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize)) { Context->SetState(PCGExMT::EState::ProcessingParams); }
		Initialize(Context->CurrentIO);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { ProcessPoint(Context->CurrentIO->In->GetPoint(i), i, Context->CurrentIO); }
		Context->SetState(PCGExMT::EState::ReadyForNextGraph);
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		//Context->OutputPointsAndParams();
		return true;
	}

#elif
	return  true;
#endif

	return false;
}

#undef LOCTEXT_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeProperties.h"

#include "Data/Blending/PCGExMetadataBlender.h"

#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeProperties

PCGExData::EInit UPCGExWriteEdgePropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }
PCGExData::EInit UPCGExWriteEdgePropertiesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExWriteEdgePropertiesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (bWriteHeuristics) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics that will be computed and written.", Required, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(WriteEdgeProperties)

FPCGExWriteEdgePropertiesContext::~FPCGExWriteEdgePropertiesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteEdgePropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExWriteEdgePropertiesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgePropertiesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExWriteEdgeProperties::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExWriteEdgeProperties::FProcessor>* NewBatch)
			{
				if (Settings->bWriteHeuristics) { NewBatch->SetRequiresHeuristics(true); }
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExWriteEdgeProperties
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MetadataBlender)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteEdgeProperties::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;

		{
			PCGExData::FFacade* OutputFacade = EdgeDataFacade;
			PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_INIT)
		}

		bSolidify = Settings->SolidificationAxis != EPCGExMinimalAxis::None;

		if (bSolidify)
		{
#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){Rad##_AXIS##Constant = Settings->Radius##_AXIS##Constant;}
			PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

			// Create edge-scope getters
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
			if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Type == EPCGExFetchType::Attribute){\
				SolidificationRad##_AXIS = Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Edge ? EdgeDataFacade->GetBroadcaster<double>(Settings->Radius##_AXIS##SourceAttribute) : VtxDataFacade->GetBroadcaster<double>(Settings->Radius##_AXIS##SourceAttribute);\
				if (!SolidificationRad##_AXIS){ PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some edges don't have the specified Radius Attribute \"{0}\"."), FText::FromName(Settings->Radius##_AXIS##SourceAttribute.GetName()))); return false; }}
			PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER

			if (Settings->SolidificationLerpOperand == EPCGExFetchType::Attribute)
			{
				SolidificationLerpGetter = EdgeDataFacade->GetBroadcaster<double>(Settings->SolidificationLerpAttribute);
				if (!SolidificationLerpGetter)
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some edges don't have the specified SolidificationEdgeLerp Attribute \"{0}\"."), FText::FromName(Settings->SolidificationLerpAttribute.GetName())));
					return false;
				}
			}
		}

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
		{
			VtxDirCompGetter = VtxDataFacade->GetBroadcaster<double>(Settings->DirSourceAttribute);
			if (!VtxDirCompGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some vtx don't have the specified DirSource Attribute \"{0}\"."), FText::FromName(Settings->DirSourceAttribute.GetName())));
				return false;
			}
		}
		else if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			EdgeDirCompGetter = EdgeDataFacade->GetBroadcaster<FVector>(Settings->DirSourceAttribute);
			if (!EdgeDirCompGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some edges don't have the specified DirSource Attribute \"{0}\"."), FText::FromName(Settings->DirSourceAttribute.GetName())));
				return false;
			}
		}

		if (Settings->bEndpointsBlending)
		{
			MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingDetails*>(&Settings->BlendingSettings));
			MetadataBlender->PrepareForData(EdgeDataFacade, VtxDataFacade, PCGExData::ESource::In);
		}

		bAscendingDesired = Settings->DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
		StartWeight = FMath::Clamp(Settings->EndpointsWeights, 0, 1);
		EndWeight = 1 - StartWeight;

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		uint32 EdgeStartPtIndex = Edge.Start;
		uint32 EdgeEndPtIndex = Edge.End;

		const FPCGPoint& StartPoint = VtxIO->GetInPoint(EdgeStartPtIndex);
		const FPCGPoint& EndPoint = VtxIO->GetInPoint(EdgeEndPtIndex);

		double BlendWeightStart = StartWeight;
		double BlendWeightEnd = EndWeight;

		FVector DirFrom = StartPoint.Transform.GetLocation();
		FVector DirTo = EndPoint.Transform.GetLocation();
		bool bAscending = true; // Default for Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder

		if (LocalSettings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
		{
			bAscending = VtxDirCompGetter->Values[EdgeStartPtIndex] < VtxDirCompGetter->Values[EdgeEndPtIndex];
		}
		else if (LocalSettings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			const FVector CounterDir = EdgeDirCompGetter->Values[Edge.EdgeIndex];
			const FVector StartEndDir = (EndPoint.Transform.GetLocation() - StartPoint.Transform.GetLocation()).GetSafeNormal();
			const FVector EndStartDir = (StartPoint.Transform.GetLocation() - EndPoint.Transform.GetLocation()).GetSafeNormal();
			bAscending = CounterDir.Dot(StartEndDir) < CounterDir.Dot(EndStartDir);
		}
		else if (LocalSettings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
		{
			bAscending = (EdgeStartPtIndex < EdgeEndPtIndex);
		}

		if (bAscending != bAscendingDesired)
		{
			std::swap(DirTo, DirFrom);
			std::swap(EdgeStartPtIndex, EdgeEndPtIndex);
		}

		const FVector EdgeDirection = (DirFrom - DirTo).GetSafeNormal();
		const double EdgeLength = FVector::Distance(DirFrom, DirTo);

		PCGEX_OUTPUT_VALUE(EdgeDirection, Edge.PointIndex, EdgeDirection);
		PCGEX_OUTPUT_VALUE(EdgeLength, Edge.PointIndex, EdgeLength);

		if (LocalSettings->bWriteHeuristics)
		{
			PCGExCluster::FNode StartNode = *(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[EdgeStartPtIndex]);
			PCGExCluster::FNode EndNode = *(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[EdgeEndPtIndex]);

			switch (LocalSettings->HeuristicsMode)
			{
			case EPCGExHeuristicsWriteMode::EndpointsOrder:
				PCGEX_OUTPUT_VALUE(Heuristics, Edge.PointIndex, HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode));
				break;
			case EPCGExHeuristicsWriteMode::Smallest:
				PCGEX_OUTPUT_VALUE(
					Heuristics, Edge.PointIndex, FMath::Min(
						HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode),
						HeuristicsHandler->GetEdgeScore(EndNode, StartNode, Edge, EndNode, StartNode)));
				break;
			case EPCGExHeuristicsWriteMode::Highest:
				PCGEX_OUTPUT_VALUE(
					Heuristics, Edge.PointIndex, FMath::Max(
						HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode),
						HeuristicsHandler->GetEdgeScore(EndNode, StartNode, Edge, EndNode, StartNode)));
				break;
			default: ;
			}
		}

		FPCGPoint& MutableTarget = EdgesIO->GetMutablePoint(Edge.PointIndex);

		auto MetadataBlend = [&]()
		{
			const PCGExData::FPointRef Target = EdgesIO->GetOutPointRef(Edge.PointIndex);
			MetadataBlender->PrepareForBlending(Target);
			MetadataBlender->Blend(Target, VtxIO->GetInPointRef(EdgeStartPtIndex), Target, BlendWeightStart);
			MetadataBlender->Blend(Target, VtxIO->GetInPointRef(EdgeEndPtIndex), Target, BlendWeightEnd);
			MetadataBlender->CompleteBlending(Target, 2, BlendWeightStart + BlendWeightEnd);
		};

		if (bSolidify)
		{
			FRotator EdgeRot;
			FVector Extents = MutableTarget.GetExtents();
			FVector TargetBoundsMin = MutableTarget.BoundsMin;
			FVector TargetBoundsMax = MutableTarget.BoundsMax;

			const double EdgeLerp = FMath::Clamp(SolidificationLerpGetter ? SolidificationLerpGetter->Values[Edge.PointIndex] : LocalSettings->SolidificationLerpConstant, 0, 1);
			const double EdgeLerpInv = 1 - EdgeLerp;
			bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
				bProcessAxis = LocalSettings->bWriteRadius##_AXIS || LocalSettings->SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
				if (bProcessAxis){\
					if (LocalSettings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
						TargetBoundsMin._AXIS = -EdgeLength * EdgeLerpInv;\
						TargetBoundsMax._AXIS = EdgeLength * EdgeLerp;\
					}else{\
						double Rad = Rad##_AXIS##Constant;\
						if(SolidificationRad##_AXIS){\
						if (LocalSettings->Radius##_AXIS##Source == EPCGExGraphValueSource::Vtx) { Rad = FMath::Lerp(SolidificationRad##_AXIS->Values[EdgeStartPtIndex], SolidificationRad##_AXIS->Values[EdgeEndPtIndex], EdgeLerp); }\
						else { Rad = SolidificationRad##_AXIS->Values[Edge.PointIndex]; }}\
						TargetBoundsMin._AXIS = -Rad;\
						TargetBoundsMax._AXIS = Rad;\
					}}

			PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

			switch (LocalSettings->SolidificationAxis)
			{
			default:
			case EPCGExMinimalAxis::X:
				EdgeRot = FRotationMatrix::MakeFromX(EdgeDirection).Rotator();
				break;
			case EPCGExMinimalAxis::Y:
				EdgeRot = FRotationMatrix::MakeFromY(EdgeDirection).Rotator();
				break;
			case EPCGExMinimalAxis::Z:
				EdgeRot = FRotationMatrix::MakeFromZ(EdgeDirection).Rotator();
				break;
			}


			BlendWeightStart = EdgeLerp;
			BlendWeightEnd = 1 - EdgeLerp;

			if (MetadataBlender) { MetadataBlend(); } // Blend first THEN apply bounds otherwise it gets overwritten

			MutableTarget.Transform = FTransform(EdgeRot, FMath::Lerp(DirTo, DirFrom, EdgeLerp), MutableTarget.Transform.GetScale3D());

			MutableTarget.BoundsMin = TargetBoundsMin;
			MutableTarget.BoundsMax = TargetBoundsMax;
		}
		else if (LocalSettings->bWriteEdgePosition)
		{
			MutableTarget.Transform.SetLocation(FMath::Lerp(DirTo, DirFrom, LocalSettings->EdgePositionLerp));
			BlendWeightStart = LocalSettings->EdgePositionLerp;
			BlendWeightEnd = 1 - LocalSettings->EdgePositionLerp;

			if (MetadataBlender) { MetadataBlend(); }
		}
	}

	void FProcessor::CompleteWork()
	{
		EdgeDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"
#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

#define PCGEX_FOREACH_EDGE_AXIS(MACRO)\
MACRO(X)\
MACRO(Y)\
MACRO(Z)


UPCGExWriteEdgeExtrasSettings::UPCGExWriteEdgeExtrasSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetMainOutputInitMode() const { return bWriteVtxNormal ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward; }

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WriteEdgeExtras)

FPCGExWriteEdgeExtrasContext::~FPCGExWriteEdgeExtrasContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DELETE)

	PCGEX_OUTPUT_DELETE(VtxNormal, FVector)
	PCGEX_DELETE(VtxDirCompGetter)
	PCGEX_DELETE(EdgeDirCompGetter)

	PCGEX_DELETE(SolidificationLerpGetter)
	PCGEX_DELETE(SolidificationRadX)
	PCGEX_DELETE(SolidificationRadY)
	PCGEX_DELETE(SolidificationRadZ)

	PCGEX_DELETE(MetadataBlender)

	ProjectionSettings.Cleanup();
}

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

	Context->bSolidify = Settings->SolidificationAxis != EPCGExMinimalAxis::None;

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_FWD)

	PCGEX_OUTPUT_VALIDATE_NAME(VtxNormal, FVector)
	PCGEX_OUTPUT_FWD(VtxNormal, FVector)

	PCGEX_FWD(DirectionChoice)
	PCGEX_FWD(DirectionMethod)

	PCGEX_FWD(bWriteEdgePosition)
	PCGEX_FWD(EdgePositionLerp)

	PCGEX_FWD(bEndpointsBlending)
	PCGEX_FWD(EndpointsWeights)

	if (Context->bEndpointsBlending)
	{
		Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
	}

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExWriteEdgeExtrasElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgeExtrasElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->VtxDirCompGetter)

#define PCGEX_CLEAN_LOCAL_PT_GETTER(_AXIS) if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point) { PCGEX_DELETE(Context->SolidificationRad##_AXIS); }
		PCGEX_FOREACH_EDGE_AXIS(PCGEX_CLEAN_LOCAL_PT_GETTER)
#undef PCGEX_CLEAN_LOCAL_PT_GETTER

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				return false;
			}

			Context->ProjectionSettings.Init(Context->CurrentIO);

#define PCGEX_CREATE_LOCAL_GETTER(_TEST, _GETTER, _SOURCE)\
			if (_TEST){\
				Context->_GETTER = new PCGEx::FLocalSingleFieldGetter();\
				Context->_GETTER->Capture(Settings->_SOURCE);\
				Context->_GETTER->Grab(*Context->CurrentIO);}

			PCGEX_CREATE_LOCAL_GETTER(
				Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute,
				VtxDirCompGetter, VtxSourceAttribute)

			if (Context->bSolidify)
			{
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS) PCGEX_CREATE_LOCAL_GETTER(Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point, SolidificationRad##_AXIS, Radius##_AXIS##SourceAttribute)
				PCGEX_FOREACH_EDGE_AXIS(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER
			}

#undef PCGEX_CREATE_LOCAL_GETTER

			PCGExData::FPointIO& PointIO = *Context->CurrentIO;
			PCGEX_OUTPUT_ACCESSOR_INIT(VtxNormal, FVector)

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->EdgeDirCompGetter)

		if (Context->bSolidify)
		{
#define PCGEX_CLEAN_LOCAL_EDGE_GETTER(_AXIS) if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Edge) { PCGEX_DELETE(Context->SolidificationRad##_AXIS); }
			PCGEX_FOREACH_EDGE_AXIS(PCGEX_CLEAN_LOCAL_EDGE_GETTER)
#undef PCGEX_CLEAN_LOCAL_EDGE_GETTER

			PCGEX_DELETE(Context->SolidificationLerpGetter)
		}

		if (!Context->AdvanceEdges(true))
		{
			PCGEX_OUTPUT_WRITE(VtxNormal, FVector)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		if (Context->bSolidify)
		{
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Edge) {\
Context->SolidificationRad##_AXIS = new PCGEx::FLocalSingleFieldGetter();\
Context->SolidificationRad##_AXIS->Capture(Settings->Radius##_AXIS##SourceAttribute);\
Context->SolidificationRad##_AXIS->Grab(*Context->CurrentEdges); }
			PCGEX_FOREACH_EDGE_AXIS(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER

			Context->SolidificationLerpGetter = new PCGEx::FLocalSingleFieldGetter();

			if (Settings->SolidificationLerpOperand == EPCGExOperandType::Attribute)
			{
				Context->SolidificationLerpGetter->Capture(Settings->SolidificationLerpAttribute);
				if (!Context->SolidificationLerpGetter->Grab(*Context->CurrentEdges))
				{
					PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Some edges don't have the specified SolidificationEdgeLerp Attribute {0}."), FText::FromName(Settings->SolidificationLerpAttribute.GetName())));
				}
			}
			else
			{
				Context->SolidificationLerpGetter->bEnabled = false;
			}
		}

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			Context->EdgeDirCompGetter = new PCGEx::FLocalVectorGetter();
			Context->EdgeDirCompGetter->Capture(Settings->EdgeSourceAttribute);
			Context->EdgeDirCompGetter->Grab(*Context->CurrentEdges);
		}

		PCGExData::FPointIO& PointIO = *Context->CurrentEdges;
		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_ACCESSOR_INIT)

		if (Context->MetadataBlender)
		{
			Context->MetadataBlender->PrepareForData(*Context->CurrentEdges, *Context->CurrentIO);
			Context->MetadataBlender->InitializeFromScratch();
		}

		///

		if (Context->VtxNormalWriter)
		{
			PCGExCluster::FClusterProjection* ProjectedCluster = new PCGExCluster::FClusterProjection(Context->CurrentCluster, &Context->ProjectionSettings);
			ProjectedCluster->Build();

			for (PCGExCluster::FNodeProjection& Vtx : ProjectedCluster->Nodes)
			{
				Vtx.ComputeNormal(Context->CurrentCluster);
				Context->VtxNormalWriter->Values[Vtx.Node->NodeIndex] = Vtx.Normal;
			}

			PCGEX_DELETE(ProjectedCluster)
		}

		Context->bAscendingDesired = Context->DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
		Context->StartWeight = FMath::Clamp(Context->EndpointsWeights, 0, 1);
		Context->EndWeight = 1 - Context->StartWeight;


		///


		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto ProcessEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->CurrentCluster->Edges[EdgeIndex];

			uint32 EdgeStartPtIndex = Edge.Start;
			uint32 EdgeEndPtIndex = Edge.End;

			const FPCGPoint& StartPoint = Context->CurrentIO->GetInPoint(EdgeStartPtIndex);
			const FPCGPoint& EndPoint = Context->CurrentIO->GetInPoint(EdgeEndPtIndex);

			double BlendWeightStart = Context->StartWeight;
			double BlendWeightEnd = Context->EndWeight;

			FVector DirFrom = StartPoint.Transform.GetLocation();
			FVector DirTo = EndPoint.Transform.GetLocation();
			bool bAscending = true; // Default for Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder

			if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
			{
				bAscending = Context->VtxDirCompGetter->SafeGet(EdgeStartPtIndex, EdgeStartPtIndex) < Context->VtxDirCompGetter->SafeGet(EdgeEndPtIndex, EdgeEndPtIndex);
			}
			else if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
			{
				const FVector CounterDir = Context->EdgeDirCompGetter->SafeGet(Edge.EdgeIndex, FVector::UpVector);
				const FVector StartEndDir = (EndPoint.Transform.GetLocation() - StartPoint.Transform.GetLocation()).GetSafeNormal();
				const FVector EndStartDir = (StartPoint.Transform.GetLocation() - EndPoint.Transform.GetLocation()).GetSafeNormal();
				bAscending = CounterDir.Dot(StartEndDir) < CounterDir.Dot(EndStartDir);
			}
			else if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
			{
				bAscending = (EdgeStartPtIndex < EdgeEndPtIndex);
			}

			const bool bInvert = bAscending != Context->bAscendingDesired;

			if (bInvert)
			{
				std::swap(DirTo, DirFrom);
				std::swap(EdgeStartPtIndex, EdgeEndPtIndex);
			}

			const FVector EdgeDirection = (DirFrom - DirTo).GetSafeNormal();
			const double EdgeLength = FVector::Distance(DirFrom, DirTo);

			PCGEX_OUTPUT_VALUE(EdgeDirection, Edge.PointIndex, EdgeDirection);
			PCGEX_OUTPUT_VALUE(EdgeLength, Edge.PointIndex, EdgeLength);

			FPCGPoint& MutableTarget = Context->CurrentEdges->GetMutablePoint(Edge.PointIndex);

			if (Context->bSolidify)
			{
				FRotator EdgeRot;
				FVector Extents = MutableTarget.GetExtents();
				FVector TargetBoundsMin = MutableTarget.BoundsMin;
				FVector TargetBoundsMax = MutableTarget.BoundsMax;

				const double EdgeLerp = FMath::Clamp(Context->SolidificationLerpGetter->SafeGet(Edge.PointIndex, Settings->SolidificationLerpConstant), 0, 1);
				const double EdgeLerpInv = 1 - EdgeLerp;
				bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
				bProcessAxis = Settings->bWriteRadius##_AXIS || Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
				if (bProcessAxis){\
					if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
						TargetBoundsMin._AXIS = -EdgeLength * EdgeLerpInv;\
						TargetBoundsMax._AXIS = EdgeLength * EdgeLerp;\
					}else if (Context->SolidificationRad##_AXIS->bEnabled){\
						double Rad = Extents._AXIS;\
						if (Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point) { Rad = FMath::Lerp(Context->SolidificationRad##_AXIS->SafeGet(EdgeStartPtIndex, Rad), Context->SolidificationRad##_AXIS->SafeGet(EdgeEndPtIndex, Rad), EdgeLerp); }\
						else { Rad = Context->SolidificationRad##_AXIS->SafeGet(Edge.PointIndex, Rad); }\
						TargetBoundsMin._AXIS = -Rad;\
						TargetBoundsMax._AXIS = Rad;\
					}}

				PCGEX_FOREACH_EDGE_AXIS(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

				MutableTarget.BoundsMin = TargetBoundsMin;
				MutableTarget.BoundsMax = TargetBoundsMax;

				switch (Settings->SolidificationAxis)
				{
				default:
				case EPCGExMinimalAxis::X:
					EdgeRot = UKismetMathLibrary::MakeRotFromX(EdgeDirection);
					break;
				case EPCGExMinimalAxis::Y:
					EdgeRot = UKismetMathLibrary::MakeRotFromY(EdgeDirection);
					break;
				case EPCGExMinimalAxis::Z:
					EdgeRot = UKismetMathLibrary::MakeRotFromZ(EdgeDirection);
					break;
				}

				MutableTarget.Transform = FTransform(EdgeRot, FMath::Lerp(DirTo, DirFrom, EdgeLerp), MutableTarget.Transform.GetScale3D());

				BlendWeightStart = EdgeLerp;
				BlendWeightEnd = 1 - EdgeLerp;
			}
			else if (Context->bWriteEdgePosition)
			{
				MutableTarget.Transform.SetLocation(FMath::Lerp(DirTo, DirFrom, Context->EdgePositionLerp));
				BlendWeightStart = Context->EdgePositionLerp;
				BlendWeightEnd = 1 - Context->EdgePositionLerp;
			}

			if (Context->MetadataBlender)
			{
				const PCGEx::FPointRef Target = Context->CurrentEdges->GetOutPointRef(Edge.PointIndex);
				Context->MetadataBlender->PrepareForBlending(Target);
				Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(EdgeStartPtIndex), Target, BlendWeightStart);
				Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(EdgeEndPtIndex), Target, BlendWeightEnd);
				Context->MetadataBlender->CompleteBlending(Target, 2, BlendWeightStart + BlendWeightEnd);
			}
		};

		if (!Context->Process(ProcessEdge, Context->CurrentCluster->Edges.Num())) { return false; }

		PCGEX_OUTPUT_WRITE(EdgeLength, double)
		PCGEX_OUTPUT_WRITE(EdgeDirection, FVector)
		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

#undef PCGEX_FOREACH_EDGE_AXIS
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

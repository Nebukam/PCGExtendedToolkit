// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"
#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

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

	PCGEX_DELETE_TARRAY(Batches)

	ProjectionSettings.Cleanup();
}

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

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
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				return false;
			}

			PCGExWriteEdgeExtras::FWriteEdgeExtrasBatch* NewBatch = new PCGExWriteEdgeExtras::FWriteEdgeExtrasBatch(Context, Context->CurrentIO, Context->TaggedEdges->Entries);
			Context->Batches.Add(NewBatch);
			PCGExClusterBatch::ScheduleBatch(Context->GetAsyncManager(), NewBatch);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		PCGExClusterBatch::CompleteBatches<PCGExWriteEdgeExtras::FWriteEdgeExtrasBatch>(Context->GetAsyncManager(), Context->Batches);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncCompletion);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncCompletion))
	{
		PCGEX_WAIT_ASYNC
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}


namespace PCGExWriteEdgeExtras
{
	FClusterEdgeProcess::FClusterEdgeProcess(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessingData(InVtx, InEdges)
	{
	}

	FClusterEdgeProcess::~FClusterEdgeProcess()
	{
		PCGEX_DELETE(MetadataBlender)

		PCGEX_DELETE(EdgeDirCompGetter)
		PCGEX_DELETE(SolidificationLerpGetter)

		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DELETE)

		// Delete owned getters
#define PCGEX_CLEAN_LOCAL_AXIS_GETTER(_AXIS) if (bOwnSolidificationRad##_AXIS) { PCGEX_DELETE(SolidificationRad##_AXIS) }
		PCGEX_FOREACH_XYZ(PCGEX_CLEAN_LOCAL_AXIS_GETTER)
#undef PCGEX_CLEAN_LOCAL_AXIS_GETTER
	}

	bool FClusterEdgeProcess::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessingData::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(WriteEdgeExtras)

		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)
		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_FWD)

		if (bSolidify)
		{
			// Create edge-scope getters
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
			if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Edge) {\
				bOwnSolidificationRad##_AXIS = true;\
				SolidificationRad##_AXIS = new PCGEx::FLocalSingleFieldGetter();\
				SolidificationRad##_AXIS->Capture(Settings->Radius##_AXIS##SourceAttribute);\
				SolidificationRad##_AXIS->Grab(*EdgesIO); }
			PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER

			SolidificationLerpGetter = new PCGEx::FLocalSingleFieldGetter();

			if (Settings->SolidificationLerpOperand == EPCGExOperandType::Attribute)
			{
				SolidificationLerpGetter->Capture(Settings->SolidificationLerpAttribute);
				if (!SolidificationLerpGetter->Grab(*EdgesIO))
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some edges don't have the specified SolidificationEdgeLerp Attribute {0}."), FText::FromName(Settings->SolidificationLerpAttribute.GetName())));
				}
			}
			else
			{
				SolidificationLerpGetter->bEnabled = false;
			}
		}

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			EdgeDirCompGetter = new PCGEx::FLocalVectorGetter();
			EdgeDirCompGetter->Capture(Settings->EdgeSourceAttribute);
			EdgeDirCompGetter->Grab(*EdgesIO);
		}

		if (Settings->bEndpointsBlending)
		{
			MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
			MetadataBlender->PrepareForData(*EdgesIO, *VtxIO, PCGExData::ESource::In, true);
		}

		PCGExData::FPointIO& PointIO = *EdgesIO;
		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_ACCESSOR_INIT)

		bAscendingDesired = Settings->DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
		StartWeight = FMath::Clamp(Settings->EndpointsWeights, 0, 1);
		EndWeight = 1 - StartWeight;

		if (VtxNormalWriter)
		{
			//TODO : For the love of whomever async this monstrosity
			PCGExCluster::FClusterProjection* ProjectedCluster = new PCGExCluster::FClusterProjection(Cluster, ProjectionSettings);
			ProjectedCluster->Build();

			for (PCGExCluster::FNodeProjection& VtxPt : ProjectedCluster->Nodes)
			{
				VtxPt.ComputeNormal(Cluster);
				VtxNormalWriter->Values[VtxPt.Node->NodeIndex] = VtxPt.Normal;
			}

			PCGEX_DELETE(ProjectedCluster)
		}

		StartParallelLoopForEdges(AsyncManager);

		return true;
	}

	void FClusterEdgeProcess::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		PCGEX_SETTINGS(WriteEdgeExtras)

		uint32 EdgeStartPtIndex = Edge.Start;
		uint32 EdgeEndPtIndex = Edge.End;

		const FPCGPoint& StartPoint = VtxIO->GetInPoint(EdgeStartPtIndex);
		const FPCGPoint& EndPoint = VtxIO->GetInPoint(EdgeEndPtIndex);

		double BlendWeightStart = StartWeight;
		double BlendWeightEnd = EndWeight;

		FVector DirFrom = StartPoint.Transform.GetLocation();
		FVector DirTo = EndPoint.Transform.GetLocation();
		bool bAscending = true; // Default for Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
		{
			bAscending = VtxDirCompGetter->SafeGet(EdgeStartPtIndex, EdgeStartPtIndex) < VtxDirCompGetter->SafeGet(EdgeEndPtIndex, EdgeEndPtIndex);
		}
		else if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			const FVector CounterDir = EdgeDirCompGetter->SafeGet(Edge.EdgeIndex, FVector::UpVector);
			const FVector StartEndDir = (EndPoint.Transform.GetLocation() - StartPoint.Transform.GetLocation()).GetSafeNormal();
			const FVector EndStartDir = (StartPoint.Transform.GetLocation() - EndPoint.Transform.GetLocation()).GetSafeNormal();
			bAscending = CounterDir.Dot(StartEndDir) < CounterDir.Dot(EndStartDir);
		}
		else if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
		{
			bAscending = (EdgeStartPtIndex < EdgeEndPtIndex);
		}

		const bool bInvert = bAscending != bAscendingDesired;

		if (bInvert)
		{
			std::swap(DirTo, DirFrom);
			std::swap(EdgeStartPtIndex, EdgeEndPtIndex);
		}

		const FVector EdgeDirection = (DirFrom - DirTo).GetSafeNormal();
		const double EdgeLength = FVector::Distance(DirFrom, DirTo);

		PCGEX_OUTPUT_VALUE(EdgeDirection, Edge.PointIndex, EdgeDirection);
		PCGEX_OUTPUT_VALUE(EdgeLength, Edge.PointIndex, EdgeLength);

		FPCGPoint& MutableTarget = EdgesIO->GetMutablePoint(Edge.PointIndex);

		if (bSolidify)
		{
			FRotator EdgeRot;
			FVector Extents = MutableTarget.GetExtents();
			FVector TargetBoundsMin = MutableTarget.BoundsMin;
			FVector TargetBoundsMax = MutableTarget.BoundsMax;

			const double EdgeLerp = FMath::Clamp(SolidificationLerpGetter->SafeGet(Edge.PointIndex, Settings->SolidificationLerpConstant), 0, 1);
			const double EdgeLerpInv = 1 - EdgeLerp;
			bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
				bProcessAxis = Settings->bWriteRadius##_AXIS || Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
				if (bProcessAxis){\
					if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
						TargetBoundsMin._AXIS = -EdgeLength * EdgeLerpInv;\
						TargetBoundsMax._AXIS = EdgeLength * EdgeLerp;\
					}else if (SolidificationRad##_AXIS->bEnabled){\
						double Rad = Extents._AXIS;\
						if (Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point) { Rad = FMath::Lerp(SolidificationRad##_AXIS->SafeGet(EdgeStartPtIndex, Rad), SolidificationRad##_AXIS->SafeGet(EdgeEndPtIndex, Rad), EdgeLerp); }\
						else { Rad = SolidificationRad##_AXIS->SafeGet(Edge.PointIndex, Rad); }\
						TargetBoundsMin._AXIS = -Rad;\
						TargetBoundsMax._AXIS = Rad;\
					}}

			PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
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
		else if (Settings->bWriteEdgePosition)
		{
			MutableTarget.Transform.SetLocation(FMath::Lerp(DirTo, DirFrom, Settings->EdgePositionLerp));
			BlendWeightStart = Settings->EdgePositionLerp;
			BlendWeightEnd = 1 - Settings->EdgePositionLerp;
		}

		if (MetadataBlender)
		{
			const PCGEx::FPointRef Target = EdgesIO->GetOutPointRef(Edge.PointIndex);
			MetadataBlender->PrepareForBlending(Target);
			MetadataBlender->Blend(Target, VtxIO->GetInPointRef(EdgeStartPtIndex), Target, BlendWeightStart);
			MetadataBlender->Blend(Target, VtxIO->GetInPointRef(EdgeEndPtIndex), Target, BlendWeightEnd);
			MetadataBlender->CompleteBlending(Target, 2, BlendWeightStart + BlendWeightEnd);
		}
	}

	void FClusterEdgeProcess::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		FClusterProcessingData::CompleteWork(AsyncManager);

		if (VtxEdgeCountWriter)
		{
			for (PCGExCluster::FNode& Node : Cluster->Nodes) { VtxEdgeCountWriter->Values[Node.PointIndex] = Node.Adjacency.Num(); }
		}

		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_WRITE)
		if (MetadataBlender) { MetadataBlender->Write(); }
	}

	//////// BATCH

	FWriteEdgeExtrasBatch::FWriteEdgeExtrasBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
		FClusterBatchProcessingData(InContext, InVtx, InEdges)
	{
	}

	FWriteEdgeExtrasBatch::~FWriteEdgeExtrasBatch()
	{
		PCGEX_SETTINGS(WriteEdgeExtras)

		PCGEX_OUTPUT_DELETE(VtxNormal, FVector)
		PCGEX_OUTPUT_DELETE(VtxEdgeCount, int32)

		PCGEX_DELETE(VtxDirCompGetter)

		PCGEX_DELETE(SolidificationRadX);
		PCGEX_DELETE(SolidificationRadY);
		PCGEX_DELETE(SolidificationRadZ);
	}

	bool FWriteEdgeExtrasBatch::PrepareProcessing()
	{
		PCGEX_SETTINGS(WriteEdgeExtras)

		if (!FClusterBatchProcessingData::PrepareProcessing()) { return false; }

		PCGEX_OUTPUT_VALIDATE_NAME(VtxNormal, FVector)
		PCGEX_OUTPUT_VALIDATE_NAME(VtxEdgeCount, int32)

		PCGEX_OUTPUT_FWD(VtxNormal, FVector)
		PCGEX_OUTPUT_FWD(VtxEdgeCount, int32)

		PCGExData::FPointIO& PointIO = *VtxIO;
		PCGEX_OUTPUT_ACCESSOR_INIT(VtxNormal, FVector)
		PCGEX_OUTPUT_ACCESSOR_INIT(VtxEdgeCount, int32)

		ProjectionSettings.Init(VtxIO);

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
		{
			VtxDirCompGetter = new PCGEx::FLocalSingleFieldGetter();
			VtxDirCompGetter->Capture(Settings->VtxSourceAttribute);
			VtxDirCompGetter->Grab(*VtxIO);
		}

		const bool bSolidify = Settings->SolidificationAxis != EPCGExMinimalAxis::None;

		if (bSolidify)
		{
			// Prepare vtx-scoped getters
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
						if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point) {\
						SolidificationRad##_AXIS = new PCGEx::FLocalSingleFieldGetter();\
						SolidificationRad##_AXIS->Capture(Settings->Radius##_AXIS##SourceAttribute);\
						SolidificationRad##_AXIS->Grab(*VtxIO); }
			PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER
		}

		return true;
	}

	bool FWriteEdgeExtrasBatch::PrepareSingle(FClusterEdgeProcess* ClusterProcessor)
	{
		PCGEX_SETTINGS(WriteEdgeExtras)

		const bool bSolidify = Settings->SolidificationAxis != EPCGExMinimalAxis::None;

		ClusterProcessor->ProjectionSettings = &ProjectionSettings;

		if (bSolidify)
		{
			// Fwd vtx-scoped getters
#define PCGEX_ASSIGN_AXIS_GETTER(_AXIS) \
			if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Source == EPCGExGraphValueSource::Point) { \
				ClusterProcessor->bOwnSolidificationRad##_AXIS = false; \
				ClusterProcessor->SolidificationRad##_AXIS = SolidificationRad##_AXIS; }
			PCGEX_FOREACH_XYZ(PCGEX_ASSIGN_AXIS_GETTER)
#undef PCGEX_ASSIGN_AXIS_GETTER
		}

		ClusterProcessor->bSolidify = bSolidify;

		ClusterProcessor->VtxNormalWriter = VtxNormalWriter;
		ClusterProcessor->VtxEdgeCountWriter = VtxEdgeCountWriter;
		ClusterProcessor->VtxDirCompGetter = VtxDirCompGetter;

		return true;
	}

	void FWriteEdgeExtrasBatch::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		FClusterBatchProcessingData<FClusterEdgeProcess>::CompleteWork(AsyncManager);

		PCGEX_OUTPUT_WRITE(VtxNormal, FVector)
		PCGEX_OUTPUT_WRITE(VtxEdgeCount, FVector)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

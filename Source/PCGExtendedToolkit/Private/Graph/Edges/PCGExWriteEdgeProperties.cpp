// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeProperties.h"

#include "Data/Blending/PCGExBlendOpFactoryProvider.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeProperties

PCGExData::EIOInit UPCGExWriteEdgePropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExWriteEdgePropertiesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExWriteEdgePropertiesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (bEndpointsBlending && BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})
	}
	else
	{
		PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations. These are currently ignored, but will preserve pin connections", Advanced, {})
	}

	if (bWriteHeuristics) { PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics that will be computed and written.", Required, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(WriteEdgeProperties)

bool FPCGExWriteEdgePropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->bEndpointsBlending && Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
			Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
			{PCGExFactories::EType::Blending}, false);
	}

	return true;
}

bool FPCGExWriteEdgePropertiesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgePropertiesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeProperties)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExWriteEdgeProperties::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExWriteEdgeProperties::FBatch>& NewBatch)
			{
				if (Settings->bWriteHeuristics) { NewBatch->SetWantsHeuristics(true); }
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExWriteEdgeProperties
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteEdgeProperties::Process);

		EdgeDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = EdgeDataFacade;
			PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_INIT)
		}

		bSolidify = Settings->SolidificationAxis != EPCGExMinimalAxis::None;

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (bSolidify)
		{
			AllocateFor |= EPCGPointNativeProperties::BoundsMin;
			AllocateFor |= EPCGPointNativeProperties::BoundsMax;
		}
		if (bSolidify || Settings->bWriteEdgePosition)
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		EdgeDataFacade->GetOut()->AllocateProperties(AllocateFor);

		if (bSolidify)
		{
#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){\
			SolidificationRad##_AXIS = PCGExDetails::MakeSettingValue(Settings->Radius##_AXIS##Input, Settings->Radius##_AXIS##SourceAttribute, Settings->Radius##_AXIS##Constant);\
			if(!SolidificationRad##_AXIS->Init(Context, Settings->Radius##_AXIS##Source == EPCGExClusterElement::Edge ? EdgeDataFacade : VtxDataFacade, false)){ return false; } }
			PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

			SolidificationLerp = Settings->GetValueSettingSolidificationLerp();
			if (!SolidificationLerp->Init(Context, EdgeDataFacade, false)) { return false; }
		}

		if (Settings->bEndpointsBlending)
		{
			if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
			{
				if (!Context->BlendingFactories.IsEmpty())
				{
					BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>(EdgeDataFacade);
					BlendOpsManager->SetSources(VtxDataFacade); // We want operands A & B to be the vtx here

					if (!BlendOpsManager->Init(Context, Context->BlendingFactories)) { return false; }
				}

				DataBlender = BlendOpsManager;
			}
			else
			{
				MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
				MetadataBlender->SetTargetData(EdgeDataFacade);
				MetadataBlender->SetSourceData(VtxDataFacade);

				if (!MetadataBlender->Init(Context, Settings->BlendingSettings))
				{
					// Fail
					Context->CancelExecution(FString("Error initializing blending"));
					return false;
				}

				DataBlender = MetadataBlender;
			}
		}

		if (!DataBlender) { DataBlender = MakeShared<PCGExDataBlending::FDummyBlender>(); }

		StartWeight = FMath::Clamp(Settings->EndpointsWeights, 0, 1);
		EndWeight = 1 - StartWeight;

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExGraph::FEdge>& ClusterEdges = *Cluster->Edges;
		EdgeDataFacade->Fetch(Scope);

		bool bUseRealRanges = bSolidify || Settings->bWriteEdgePosition;

		TPCGValueRange<FTransform> Transforms = bUseRealRanges ? EdgeDataFacade->GetOut()->GetTransformValueRange(false) : TPCGValueRange<FTransform>();
		TPCGValueRange<FVector> BoundsMin = bUseRealRanges ? EdgeDataFacade->GetOut()->GetBoundsMinValueRange(false) : TPCGValueRange<FVector>();
		TPCGValueRange<FVector> BoundsMax = bUseRealRanges ? EdgeDataFacade->GetOut()->GetBoundsMaxValueRange(false) : TPCGValueRange<FVector>();

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraph::FEdge& Edge = ClusterEdges[Index];
			const int32 EdgeIndex = Edge.PointIndex;

			DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

			const PCGExCluster::FNode& StartNode = *Cluster->GetEdgeStart(Edge);
			const PCGExCluster::FNode& EndNode = *Cluster->GetEdgeEnd(Edge);

			const FVector A = Cluster->GetPos(StartNode);
			const FVector B = Cluster->GetPos(EndNode);

			const FVector EdgeDirection = (A - B).GetSafeNormal();
			const double EdgeLength = FVector::Distance(A, B);

			PCGEX_OUTPUT_VALUE(EdgeDirection, EdgeIndex, EdgeDirection);
			PCGEX_OUTPUT_VALUE(EdgeLength, EdgeIndex, EdgeLength);

			if (Settings->bWriteHeuristics)
			{
				switch (Settings->HeuristicsMode)
				{
				case EPCGExHeuristicsWriteMode::EndpointsOrder:
					PCGEX_OUTPUT_VALUE(Heuristics, EdgeIndex, HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode));
					break;
				case EPCGExHeuristicsWriteMode::Smallest:
					PCGEX_OUTPUT_VALUE(
						Heuristics, EdgeIndex, FMath::Min(
							HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode),
							HeuristicsHandler->GetEdgeScore(EndNode, StartNode, Edge, EndNode, StartNode)));
					break;
				case EPCGExHeuristicsWriteMode::Highest:
					PCGEX_OUTPUT_VALUE(
						Heuristics, EdgeIndex, FMath::Max(
							HeuristicsHandler->GetEdgeScore(StartNode, EndNode, Edge, StartNode, EndNode),
							HeuristicsHandler->GetEdgeScore(EndNode, StartNode, Edge, EndNode, StartNode)));
					break;
				default: ;
				}
			}

			if (bSolidify)
			{
				FRotator EdgeRot;
				FVector TargetBoundsMin = BoundsMin[EdgeIndex];
				FVector TargetBoundsMax = BoundsMax[EdgeIndex];

				FVector TargetScale = Transforms[EdgeIndex].GetScale3D();

				const FVector InvScale = FVector::One() / TargetScale;

				double BlendWeightStart = FMath::Clamp(SolidificationLerp->Read(EdgeIndex), 0, 1);
				double BlendWeightEnd = 1 - BlendWeightStart;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
TargetBoundsMin._AXIS = (-EdgeLength * BlendWeightEnd) * InvScale._AXIS;\
TargetBoundsMax._AXIS = (EdgeLength * BlendWeightStart) * InvScale._AXIS;\
}else if(SolidificationRad##_AXIS){\
double Rad = 0;\
if (Settings->Radius##_AXIS##Source == EPCGExClusterElement::Vtx) { Rad = FMath::Lerp(SolidificationRad##_AXIS->Read(Edge.Start), SolidificationRad##_AXIS->Read(Edge.End), BlendWeightStart); }\
else { Rad = SolidificationRad##_AXIS->Read(EdgeIndex); }\
TargetBoundsMin._AXIS = -Rad * InvScale._AXIS;\
TargetBoundsMax._AXIS = Rad * InvScale._AXIS;\
}

				PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

				switch (Settings->SolidificationAxis)
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

				Transforms[EdgeIndex] = FTransform(EdgeRot, FMath::Lerp(B, A, BlendWeightEnd), TargetScale);

				BoundsMin[EdgeIndex] = TargetBoundsMin;
				BoundsMax[EdgeIndex] = TargetBoundsMax;

				DataBlender->Blend(Edge.Start, Edge.End, EdgeIndex, BlendWeightEnd);
			}
			else if (Settings->bWriteEdgePosition)
			{
				Transforms[EdgeIndex].SetLocation(FMath::Lerp(B, A, Settings->EdgePositionLerp));

				DataBlender->Blend(Edge.Start, Edge.End, EdgeIndex, Settings->EdgePositionLerp);
			}
			else
			{
				DataBlender->Blend(Edge.Start, Edge.End, EdgeIndex, Settings->EdgePositionLerp);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (BlendOpsManager) { BlendOpsManager->Cleanup(Context); }
		EdgeDataFacade->Write(AsyncManager);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExWriteEdgePropertiesContext, UPCGExWriteEdgePropertiesSettings>::Cleanup();
		BlendOpsManager.Reset();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

		Settings->BlendingSettings.RegisterBuffersDependencies(Context, FacadePreloader);
		PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, FacadePreloader, Context->BlendingFactories);
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteEdgeProperties)

		DirectionSettings = Settings->DirectionSettings;

		if (!DirectionSettings.Init(ExecutionContext, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			bIsBatchValid = false;
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

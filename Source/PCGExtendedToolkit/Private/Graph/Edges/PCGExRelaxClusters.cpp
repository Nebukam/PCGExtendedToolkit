// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRelaxClusters.h"


#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"

#define LOCTEXT_NAMESPACE "PCGExRelaxClusters"
#define PCGEX_NAMESPACE RelaxClusters

PCGExData::EIOInit UPCGExRelaxClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGExData::EIOInit UPCGExRelaxClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExRelaxClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExRelaxClusters::SourceOverridesRelaxing)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(RelaxClusters)

bool FPCGExRelaxClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)
	PCGEX_FOREACH_FIELD_RELAX_CLUSTER(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_OPERATION_BIND(Relaxing, UPCGExRelaxClusterOperation, PCGExRelaxClusters::SourceOverridesRelaxing)

	return true;
}

bool FPCGExRelaxClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExRelaxClusters::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExRelaxClusters::FBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExRelaxClusters
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, VtxDataFacade->Source, NodeIndexLookup,
			true, false, false);
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRelaxClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		InfluenceDetails = Settings->InfluenceDetails;
		if (!InfluenceDetails.Init(ExecutionContext, VtxDataFacade)) { return false; }

		RelaxOperation = Context->Relaxing->CopyOperation<UPCGExRelaxClusterOperation>();
		RelaxOperation->PrimaryDataFacade = VtxDataFacade;
		RelaxOperation->SecondaryDataFacade = EdgeDataFacade;

		if (!RelaxOperation->PrepareForCluster(Cluster)) { return false; }

		PrimaryBuffer = MakeShared<TArray<FTransform>>();
		SecondaryBuffer = MakeShared<TArray<FTransform>>();

		PrimaryBuffer->Init(FTransform::Identity, NumNodes);
		SecondaryBuffer->Init(FTransform::Identity, NumNodes);

		TArray<FTransform>& PBufferRef = (*PrimaryBuffer);
		TArray<FTransform>& SBufferRef = (*SecondaryBuffer);

		const TArray<FPCGPoint>& Vtxs = VtxDataFacade->GetIn()->GetPoints();
		for (int i = 0; i < NumNodes; i++) { PBufferRef[i] = SBufferRef[i] = Vtxs[Cluster->GetNode(i)->PointIndex].Transform; }

		RelaxOperation->ReadBuffer = PrimaryBuffer.Get();
		RelaxOperation->WriteBuffer = SecondaryBuffer.Get();

		Iterations = Settings->Iterations;

		Steps = RelaxOperation->GetNumSteps();
		CurrentStep = -1;
		StartNextStep();
		return true;
	}

	void FProcessor::StartNextStep()
	{
		CurrentStep++;

		if (Iterations <= 0)
		{
			// Wrap up
			StartParallelLoopForNodes();
			return;
		}

		if (CurrentStep > Steps)
		{
			Iterations--;
			CurrentStep = 0;
		}

		StepSource = RelaxOperation->PrepareNextStep(CurrentStep);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, IterationGroup)

		IterationGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartNextStep();
			};

		IterationGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->RelaxScope(Scope);
			};

		switch (StepSource)
		{
		case EPCGExClusterComponentSource::Vtx:
			IterationGroup->StartSubLoops(NumNodes, 32);
			break;
		case EPCGExClusterComponentSource::Edge:
			IterationGroup->StartSubLoops(NumEdges, 32);
			break;
		}
	}

	void FProcessor::RelaxScope(const PCGExMT::FScope& Scope) const
	{
		const TArray<FTransform>& RBufferRef = (*RelaxOperation->ReadBuffer);
		TArray<FTransform>& WBufferRef = (*RelaxOperation->WriteBuffer);

#define PCGEX_RELAX_PROGRESS WBufferRef[i] = PCGExMath::Lerp( RBufferRef[i], WBufferRef[i], InfluenceDetails.GetInfluence(Node.PointIndex));
#define PCGEX_RELAX_STEP_NODE(_STEP) if (CurrentStep == _STEP-1){if(bLastStep){ \
		for (int i = Scope.Start; i < Scope.End; i++){ PCGExCluster::FNode& Node = *Cluster->GetNode(i); RelaxOperation->Step##_STEP(Node); PCGEX_RELAX_PROGRESS } \
		}else{ for (int i = Scope.Start; i < Scope.End; i++){ RelaxOperation->Step##_STEP(*Cluster->GetNode(i)); }} return; }

#define PCGEX_RELAX_STEP_EDGE(_STEP) if (CurrentStep == _STEP-1){ for (int i = Scope.Start; i < Scope.End; i++){ RelaxOperation->Step##_STEP(*Cluster->GetEdge(i)); } return; }

		const bool bLastStep = (CurrentStep == Steps) && InfluenceDetails.bProgressiveInfluence;

		switch (StepSource)
		{
		case EPCGExClusterComponentSource::Vtx:
			PCGEX_RELAX_STEP_NODE(1)
			PCGEX_RELAX_STEP_NODE(2)
			PCGEX_RELAX_STEP_NODE(3)
			break;
		case EPCGExClusterComponentSource::Edge:
			PCGEX_RELAX_STEP_EDGE(1)
			PCGEX_RELAX_STEP_EDGE(2)
			PCGEX_RELAX_STEP_EDGE(3)
			break;
		}

#undef PCGEX_RELAX_PROGRESS
#undef PCGEX_RELAX_STEP_NODE
#undef PCGEX_RELAX_STEP_EDGE
	}

	void FProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExRelaxClustersContext, UPCGExRelaxClustersSettings>::PrepareLoopScopesForNodes(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope)
	{
		// Commit values
		FPCGPoint& Point = VtxDataFacade->Source->GetMutablePoint(Node.PointIndex);

		TArray<FPCGPoint>& MutablePoints = VtxDataFacade->GetOut()->GetMutablePoints();
		if (!InfluenceDetails.bProgressiveInfluence)
		{
			Point.Transform = PCGExMath::Lerp(
				Point.Transform,
				*(RelaxOperation->WriteBuffer->GetData() + Node.Index),
				InfluenceDetails.GetInfluence(Node.PointIndex));
		}
		else
		{
			Point.Transform = *(RelaxOperation->WriteBuffer->GetData() + Node.Index);
		}

		const FVector DirectionAndSize = Point.Transform.GetLocation() - Cluster->GetPos(Node.Index);

		PCGEX_OUTPUT_VALUE(DirectionAndSize, Node.PointIndex, DirectionAndSize)
		PCGEX_OUTPUT_VALUE(Direction, Node.PointIndex, DirectionAndSize.GetSafeNormal())
		PCGEX_OUTPUT_VALUE(Amplitude, Node.PointIndex, DirectionAndSize.Length())
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		TProcessor<FPCGExRelaxClustersContext, UPCGExRelaxClustersSettings>::OnNodesProcessingComplete();
		Cluster->WillModifyVtxPositions(true);
		ForwardCluster();
	}

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		GetContext<FPCGExRelaxClustersContext>()->Relaxing->RegisterPrimaryBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RelaxClusters)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;
			PCGEX_FOREACH_FIELD_RELAX_CLUSTER(PCGEX_OUTPUT_INIT)
		}
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }

#define PCGEX_OUTPUT_FWD_TO(_NAME, _TYPE, _DEFAULT_VALUE) if(_NAME##Writer){ ClusterProcessor->_NAME##Writer = _NAME##Writer; }
		PCGEX_FOREACH_FIELD_RELAX_CLUSTER(PCGEX_OUTPUT_FWD_TO)
#undef PCGEX_OUTPUT_FWD_TO

		return true;
	}

	void FBatch::Write()
	{
		TBatch<FProcessor>::Write();
		VtxDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

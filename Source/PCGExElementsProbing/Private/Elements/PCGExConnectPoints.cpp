// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExConnectPoints.h"

#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExPointFilter.h"
#include "Data/PCGExClusterData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Graphs/PCGExGraph.h"
#include "Core/PCGExProbeFactoryProvider.h"
#include "Core/PCGExProbeOperation.h"
#include "Core/PCGExProbingCandidates.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "PCGExConnectPointsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExClusters::Labels::SourceProbesLabel, "Probes used to connect points", Required, FPCGExDataTypeInfoProbe::AsId())

	PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceFilterGenerators, "Points that don't meet requirements won't generate connections", Normal)
	PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceFilterConnectables, "Points that don't meet requirements can't receive connections", Normal)

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExClusters::Labels::OutputEdgesLabel, "Point data representing edges.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ConnectPoints)
PCGEX_ELEMENT_BATCH_POINT_IMPL(ConnectPoints)

bool FPCGExConnectPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (!PCGExFactories::GetInputFactories<UPCGExProbeFactoryData>(Context, PCGExClusters::Labels::SourceProbesLabel, Context->ProbeFactories, {PCGExFactories::EType::Probe}))
	{
		return false;
	}

	GetInputFactories(Context, PCGExClusters::Labels::SourceFilterGenerators, Context->GeneratorsFiltersFactories, PCGExFactories::PointFilters, false);
	GetInputFactories(Context, PCGExClusters::Labels::SourceFilterConnectables, Context->ConnectablesFiltersFactories, PCGExFactories::PointFilters, false);

	Context->CWCoincidenceTolerance = FVector(Settings->CoincidenceTolerance);

	return true;
}

bool FPCGExConnectPointsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters. Make sure inputs have at least 2 points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	Context->MainBatch->Output();

	return Context->TryComplete();
}

namespace PCGExConnectPoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConnectPoints::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const int32 NumPoints = PointDataFacade->GetNum();

		CWCoincidenceTolerance = PCGEx::SafeTolerance(Context->CWCoincidenceTolerance);
		bPreventCoincidence = Settings->bPreventCoincidence;

		if (Settings->bProjectPoints)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { ProjectionDetails.Init(PointDataFacade); }
			else { ProjectionDetails.Init(PCGExMath::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }
		}

		for (const UPCGExProbeFactoryData* Factory : Context->ProbeFactories)
		{
			TSharedPtr<FPCGExProbeOperation> NewOperation = Factory->CreateOperation(Context);
			NewOperation->BindContext(ExecutionContext);
			NewOperation->PrimaryDataFacade = PointDataFacade;

			if (!NewOperation->PrepareForPoints(Context, PointDataFacade->Source)) { continue; }

			if (!NewOperation->RequiresOctree())
			{
				DirectOperations.Add(NewOperation);
				continue;
			}

			if (!NewOperation->SearchRadius->IsConstant()) { bUseVariableRadius = true; }
			SharedSearchRadius = FMath::Max(SharedSearchRadius, NewOperation->BaseConfig->SearchRadiusConstant);

			if (NewOperation->RequiresChainProcessing()) { ChainedOperations.Add(NewOperation); }
			else { SharedOperations.Add(NewOperation); }

			RadiusSources.Add(NewOperation);
		}

		NumRadiusSources = RadiusSources.Num();
		NumChainedOps = ChainedOperations.Num();
		NumSharedOps = SharedOperations.Num();
		NumDirectOps = DirectOperations.Num();

		if (RadiusSources.IsEmpty() && DirectOperations.IsEmpty()) { return false; }

		if (!PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New)) { return false; }
		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);

		CanGenerate.SetNumUninitialized(NumPoints);
		AcceptConnections.SetNumUninitialized(NumPoints);

		PCGExArrayHelpers::InitArray(WorkingTransforms, NumPoints);

		if (!Context->GeneratorsFiltersFactories.IsEmpty())
		{
			GeneratorsFilter = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!GeneratorsFilter->Init(ExecutionContext, Context->GeneratorsFiltersFactories)) { return false; }
		}

		if (!Context->ConnectablesFiltersFactories.IsEmpty())
		{
			ConnectableFilter = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!ConnectableFilter->Init(ExecutionContext, Context->ConnectablesFiltersFactories)) { return false; }
		}

		bUseProjection = Settings->bProjectPoints;

		if (!RadiusSources.IsEmpty())
		{
			// If we have search probes, build the octree
			const FBox B = PointDataFacade->GetIn()->GetBounds();
			Octree = MakeUnique<PCGExOctree::FItemOctree>(bUseProjection ? ProjectionDetails.ProjectFlat(B.GetCenter()) : B.GetCenter(), B.GetExtent().Length());
		}

		PCGEX_ASYNC_GROUP_CHKD(TaskManager, PrepTask)

		PrepTask->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->OnPreparationComplete();
		};

		PrepTask->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			This->PointDataFacade->Fetch(Scope);

			PCGEX_SCOPE_LOOP(i)
			{
				This->CanGenerate[i] = This->GeneratorsFilter ? This->GeneratorsFilter->Test(i) : true;
				This->AcceptConnections[i] = This->ConnectableFilter ? This->ConnectableFilter->Test(i) : true;
			}
		};

		PrepTask->StartSubLoops(NumPoints, PCGEX_CORE_SETTINGS.GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::OnPreparationComplete()
	{
		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		const TConstPCGValueRange<FTransform> OriginalTransforms = InPointData->GetConstTransformValueRange();

		const int32 NumPoints = OriginalTransforms.Num();

		if (!RadiusSources.IsEmpty())
		{
			constexpr double PPRefRadius = 0.05;
			const FVector PPRefExtents = FVector(PPRefRadius);

			if (bUseProjection)
			{
				for (int i = 0; i < NumPoints; i++)
				{
					WorkingTransforms[i] = ProjectionDetails.ProjectFlat(OriginalTransforms[i], i);
					if (!AcceptConnections[i]) { continue; }
					Octree->AddElement(PCGExOctree::FItem(i, FBoxSphereBounds(WorkingTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
			else
			{
				for (int i = 0; i < NumPoints; i++)
				{
					WorkingTransforms[i] = OriginalTransforms[i];
					if (!AcceptConnections[i]) { continue; }
					Octree->AddElement(PCGExOctree::FItem(i, FBoxSphereBounds(WorkingTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
		}

		GeneratorsFilter.Reset();
		ConnectableFilter.Reset();

		StartParallelLoopForPoints(PCGExData::EIOSide::In);
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		IProcessor::PrepareLoopScopesForPoints(Loops);
		ScopedEdges = MakeShared<PCGExMT::TScopedSet<uint64>>(Loops, 10);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ConnectPoints::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		const TConstPCGValueRange<FTransform> OriginalTransforms = InPointData->GetConstTransformValueRange();

		TSet<uint64>* UniqueEdges = ScopedEdges->Get(Scope).Get();
		TUniquePtr<TSet<uint64>> LocalCoincidence;
		if (bPreventCoincidence) { LocalCoincidence = MakeUnique<TSet<uint64>>(); }

#define PCGEX_SCOPED_CONTAINERS(_NAME)\
		TArray<TSharedPtr<PCGExMT::FScopedContainer>> _NAME##OpsContainers;\
		_NAME##OpsContainers.Reserve(NumChainedOps);\
		for (int i = 0; i < Num##_NAME##Ops; i++){ _NAME##OpsContainers.Add(_NAME##Operations[i]->GetScopedContainer(Scope)); }

		PCGEX_SCOPED_CONTAINERS(Chained)
		PCGEX_SCOPED_CONTAINERS(Shared)
		PCGEX_SCOPED_CONTAINERS(Direct)

#undef PCGEX_SCOPED_CONTAINERS

		TArray<PCGExProbing::FCandidate> Candidates;
		TArray<PCGExProbing::FBestCandidate> BestCandidates;

		FTransform CandidateTransform = FTransform::Identity;
		FVector Origin = FVector::ZeroVector;
		int32 CurrentIndex = 0;

		auto ProcessPoint = [&](const PCGExOctree::FItem& InPositionRef)
		{
			const int32 OtherPointIndex = InPositionRef.Index;
			if (OtherPointIndex == CurrentIndex) { return; }

			const FVector Position = WorkingTransforms[OtherPointIndex].GetLocation();
			const FVector Dir = (Origin - Position).GetSafeNormal();
			const int32 EmplaceIndex = Candidates.Emplace(OtherPointIndex, Dir, FVector::DistSquared(Position, Origin), bPreventCoincidence ? PCGEx::SH3(Dir, CWCoincidenceTolerance) : 0);

			if (NumChainedOps > 0)
			{
				for (int i = 0; i < NumChainedOps; i++)
				{
					ChainedOperations[i]->ProcessCandidateChained(i, CandidateTransform, EmplaceIndex, Candidates[EmplaceIndex], BestCandidates[i], ChainedOpsContainers[i].Get());
				}
			}
		};

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!CanGenerate[Index]) { continue; } // Not a generator

#define PCGEX_SCOPED_CONTAINERS_RESET(_NAME) for (const TSharedPtr<PCGExMT::FScopedContainer>& Container : _NAME##OpsContainers){ if(Container){ Container->Reset(); } }

			PCGEX_SCOPED_CONTAINERS_RESET(Chained)
			PCGEX_SCOPED_CONTAINERS_RESET(Shared)
			PCGEX_SCOPED_CONTAINERS_RESET(Direct)

#undef PCGEX_SCOPED_CONTAINERS_RESET

			CurrentIndex = Index;
			Candidates.Reset();

			if (LocalCoincidence) { LocalCoincidence->Reset(); }

			CandidateTransform = bUseProjection ? WorkingTransforms[Index] : OriginalTransforms[Index];

			if (NumChainedOps > 0)
			{
				BestCandidates.SetNum(NumChainedOps);
				for (int i = 0; i < NumChainedOps; i++)
				{
					ChainedOperations[i]->PrepareBestCandidate(Index, CandidateTransform, BestCandidates[i], ChainedOpsContainers[i].Get());
				}
			}

			if (!RadiusSources.IsEmpty())
			{
				double MaxRadius = 0;
				if (!bUseVariableRadius) { MaxRadius = SharedSearchRadius; }
				else { for (int i = 0; i < NumRadiusSources; i++) { MaxRadius = FMath::Max(MaxRadius, RadiusSources[i]->GetSearchRadius(Index)); } }

				Origin = WorkingTransforms[Index].GetLocation();

				// Find candidates within radius
				Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(MaxRadius)), ProcessPoint);
				Candidates.Sort([&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });

				for (int i = 0; i < NumChainedOps; i++)
				{
					ChainedOperations[i]->ProcessBestCandidate(Index, CandidateTransform, BestCandidates[i], Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges, ChainedOpsContainers[i].Get());
				}

				for (int i = 0; i < NumSharedOps; i++)
				{
					SharedOperations[i]->ProcessCandidates(Index, CandidateTransform, Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges, SharedOpsContainers[i].Get());
				}
			}

			for (int i = 0; i < NumDirectOps; i++)
			{
				DirectOperations[i]->ProcessNode(Index, CandidateTransform, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges, AcceptConnections, DirectOpsContainers[i].Get());
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		TSet<uint64> UniqueEdges;
		ScopedEdges->Collapse(UniqueEdges);
		GraphBuilder->Graph->InsertEdges_Unsafe(UniqueEdges, -1);
		ScopedEdges.Reset();

		GraphBuilder->CompileAsync(TaskManager, false);
	}

	void FProcessor::CompleteWork()
	{
		// Only required for post-processing work
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		PointDataFacade->WriteFastest(TaskManager);
	}

	void FProcessor::Output()
	{
		GraphBuilder->StageEdgesOutputs();
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExConnectPointsContext, UPCGExConnectPointsSettings>::Cleanup();

		RadiusSources.Empty();
		DirectOperations.Empty();
		ChainedOperations.Empty();
		SharedOperations.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

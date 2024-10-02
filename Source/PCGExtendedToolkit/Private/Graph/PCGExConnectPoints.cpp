// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConnectPoints.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExCompoundHelpers.h"
#include "Graph/Probes/PCGExProbeFactoryProvider.h"
#include "Graph/Probes/PCGExProbeOperation.h"
#include "Graph/Probes/PCGExProbing.h"

#define LOCTEXT_NAMESPACE "PCGExConnectPointsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExGraph::SourceProbesLabel, "Probes used to connect points", Required, {})

	PCGEX_PIN_PARAMS(PCGExGraph::SourceFilterGenerators, "Points that don't meet requirements won't generate connections", Normal, {})
	PCGEX_PIN_PARAMS(PCGExGraph::SourceFilterConnectables, "Points that don't meet requirements can't receive connections", Normal, {})

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExConnectPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(ConnectPoints)

bool FPCGExConnectPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (!PCGExFactories::GetInputFactories<UPCGExProbeFactoryBase>(
		Context, PCGExGraph::SourceProbesLabel, Context->ProbeFactories,
		{PCGExFactories::EType::Probe}, true))
	{
		return false;
	}

	GetInputFactories(
		Context, PCGExGraph::SourceFilterGenerators, Context->GeneratorsFiltersFactories,
		PCGExFactories::PointFilters, false);
	GetInputFactories(
		Context, PCGExGraph::SourceFilterConnectables, Context->ConnectablesFiltersFactories,
		PCGExFactories::PointFilters, false);

	Context->CWCoincidenceTolerance = FVector(1 / Settings->CoincidenceTolerance);

	return true;
}

bool FPCGExConnectPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return Entry->GetNum() >= 2; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExConnectPoints
{
	FProcessor::~FProcessor()
	{
		for (UPCGExProbeOperation* Op : ProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }
		for (UPCGExProbeOperation* Op : DirectProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConnectPoints::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const int32 NumPoints = PointDataFacade->GetNum();

		CWCoincidenceTolerance = Context->CWCoincidenceTolerance;
		bPreventCoincidence = Settings->bPreventCoincidence;

		if (Settings->bProjectPoints)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			ProjectionDetails.Init(ExecutionContext, PointDataFacade);
		}

		for (const UPCGExProbeFactoryBase* Factory : Context->ProbeFactories)
		{
			UPCGExProbeOperation* NewOperation = Factory->CreateOperation(Context);
			NewOperation->BindContext(ExecutionContext);
			NewOperation->PrimaryDataFacade = PointDataFacade;

			if (!NewOperation->PrepareForPoints(PointDataFacade->Source))
			{
				PCGEX_DELETE_UOBJECT(NewOperation)
				continue;
			}

			if (NewOperation->RequiresDirectProcessing())
			{
				DirectProbeOperations.Add(NewOperation);
				continue;
			}

			if (NewOperation->SearchRadius == -1) { bUseVariableRadius = true; }
			SharedSearchRadius = FMath::Max(SharedSearchRadius, NewOperation->SearchRadius);

			if (NewOperation->RequiresChainProcessing()) { ChainProbeOperations.Add(NewOperation); }
			else { SharedProbeOperations.Add(NewOperation); }

			ProbeOperations.Add(NewOperation);
		}

		NumChainedOps = ChainProbeOperations.Num();

		if (ProbeOperations.IsEmpty() && DirectProbeOperations.IsEmpty()) { return false; }

		PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(Context, PCGExData::EInit::NewOutput);
		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails, 2);

		CanGenerate.Init(true, NumPoints);
		PCGEx::InitArray(CachedTransforms, NumPoints);

		if (!Context->GeneratorsFiltersFactories.IsEmpty())
		{
			GeneratorsFilter = MakeUnique<PCGExPointFilter::TManager>(PointDataFacade);
			GeneratorsFilter->Init(ExecutionContext, Context->GeneratorsFiltersFactories);
		}

		if (!Context->ConnectablesFiltersFactories.IsEmpty())
		{
			ConnectableFilter = MakeUnique<PCGExPointFilter::TManager>(PointDataFacade);
			ConnectableFilter->Init(ExecutionContext, Context->ConnectablesFiltersFactories);
		}

		bUseProjection = Settings->bProjectPoints;

		InPoints = &PointDataFacade->GetIn()->GetPoints();

		if (!ProbeOperations.IsEmpty())
		{
			const FBox B = PointDataFacade->GetIn()->GetBounds();
			Octree = MakeUnique<PositionOctree>(bUseProjection ? ProjectionDetails.ProjectFlat(B.GetCenter()) : B.GetCenter(), B.GetExtent().Length());
		}
		else
		{
			if (GeneratorsFilter) { for (int i = 0; i < InPoints->Num(); ++i) { CanGenerate[i] = GeneratorsFilter->Test(i); } }
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, PrepTask)
		PrepTask->OnCompleteCallback = [&]() { OnPreparationComplete(); };
		PrepTask->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
			};

		PrepTask->PrepareRangesOnly(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::OnPreparationComplete()
	{
		const TArray<FPCGPoint>& InPointsRef = *InPoints;
		const int32 NumPoints = InPointsRef.Num();

		if (!ProbeOperations.IsEmpty())
		{
			constexpr double PPRefRadius = 0.05;
			const FVector PPRefExtents = FVector(PPRefRadius);

			if (bUseProjection)
			{
				for (int i = 0; i < NumPoints; ++i)
				{
					CachedTransforms[i] = ProjectionDetails.ProjectFlat(InPointsRef[i].Transform, i);

					CanGenerate[i] = GeneratorsFilter ? GeneratorsFilter->Test(i) : true;
					if (ConnectableFilter && ConnectableFilter->Test(i)) { continue; }
					Octree->AddElement(FPositionRef(i, FBoxSphereBounds(CachedTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
			else
			{
				for (int i = 0; i < NumPoints; ++i)
				{
					CachedTransforms[i] = InPointsRef[i].Transform;

					CanGenerate[i] = GeneratorsFilter ? GeneratorsFilter->Test(i) : true;
					if (ConnectableFilter && !ConnectableFilter->Test(i)) { continue; }
					Octree->AddElement(FPositionRef(i, FBoxSphereBounds(CachedTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
		}

		GeneratorsFilter.Reset();
		ConnectableFilter.Reset();

		StartParallelLoopForPoints(PCGExData::ESource::In);
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<uint64>& Loops)
	{
		FPointsProcessor::PrepareLoopScopesForPoints(Loops);
		for (int i = 0; i < Loops.Num(); ++i) { DistributedEdgesSet.Add(MakeShared<TSet<uint64>>()); }
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::ProcessSinglePoint);

		if (!CanGenerate[Index]) { return; } // Not a generator

		TSharedPtr<TSet<uint64>> UniqueEdges = DistributedEdgesSet[LoopIdx];
		TUniquePtr<TSet<FInt32Vector>> LocalCoincidence;
		if (bPreventCoincidence) { LocalCoincidence = MakeUnique<TSet<FInt32Vector>>(); }

		FPCGPoint PointCopy = Point;

		if (bUseProjection)
		{
			// Adjust local point transform with projection
			PointCopy.Transform = CachedTransforms[Index];
		}

		TArray<PCGExProbing::FBestCandidate> BestCandidates;

		if (NumChainedOps > 0)
		{
			BestCandidates.SetNum(NumChainedOps);
			for (int i = 0; i < NumChainedOps; ++i) { ChainProbeOperations[i]->PrepareBestCandidate(Index, PointCopy, BestCandidates[i]); }
		}

		if (!ProbeOperations.IsEmpty())
		{
			double MaxRadius = MIN_dbl;
			if (!bUseVariableRadius) { MaxRadius = SharedSearchRadius; }
			else
			{
				for (UPCGExProbeOperation* Op : ProbeOperations) { MaxRadius = FMath::Max(MaxRadius, Op->SearchRadiusCache ? Op->SearchRadiusCache->Read(Index) : Op->SearchRadius); }
			}

			const FVector Origin = CachedTransforms[Index].GetLocation();

			TArray<PCGExProbing::FCandidate> Candidates;

			auto ProcessPoint = [&](const FPositionRef& InPositionRef)
			{
				const int32 OtherPointIndex = InPositionRef.Index;
				if (OtherPointIndex == Index) { return; }

				const FVector Position = CachedTransforms[OtherPointIndex].GetLocation();
				const FVector Dir = (Origin - Position).GetSafeNormal();
				const int32 EmplaceIndex = Candidates.Emplace(
					OtherPointIndex,
					Dir,
					FVector::DistSquared(Position, Origin),
					bPreventCoincidence ? PCGEx::I323(Dir, CWCoincidenceTolerance) : FInt32Vector::ZeroValue);

				if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; ++i) { ChainProbeOperations[i]->ProcessCandidateChained(i, PointCopy, EmplaceIndex, Candidates[EmplaceIndex], BestCandidates[i]); } }
			};

			Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(MaxRadius)), ProcessPoint);

			if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; ++i) { ChainProbeOperations[i]->ProcessBestCandidate(Index, PointCopy, BestCandidates[i], Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges.Get()); } }

			if (!Candidates.IsEmpty())
			{
				Algo::Sort(Candidates, [&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, PointCopy, Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges.Get()); }
			}
			else
			{
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, PointCopy, Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges.Get()); }
			}
		}

		for (UPCGExProbeOperation* Op : DirectProbeOperations) { Op->ProcessNode(Index, PointCopy, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges.Get()); }
	}

	void FProcessor::CompleteWork()
	{
		for (TSharedPtr<TSet<uint64>>& EdgesSet : DistributedEdgesSet)
		{
			GraphBuilder->Graph->InsertEdgesUnsafe(*EdgesSet, -1);
			EdgesSet.Reset();
		}

		DistributedEdgesSet.Empty();

		GraphBuilder->CompileAsync(AsyncManager, false);
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			return;
		}

		PointDataFacade->Write(AsyncManager);
		GraphBuilder->OutputEdgesToContext();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

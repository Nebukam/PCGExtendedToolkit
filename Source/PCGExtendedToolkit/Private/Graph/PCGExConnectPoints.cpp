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

FPCGExConnectPointsContext::~FPCGExConnectPointsContext()
{
	PCGEX_TERMINATE_ASYNC

	ProbeFactories.Empty();
	GeneratorsFiltersFactories.Empty();
	ConnetablesFiltersFactories.Empty();

	PCGEX_DELETE(MainVtx);
}

bool FPCGExConnectPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExGraph::SourceProbesLabel, Context->ProbeFactories,
		{PCGExFactories::EType::Probe}, true))
	{
		return false;
	}

	GetInputFactories(
		Context, PCGExGraph::SourceFilterGenerators, Context->GeneratorsFiltersFactories,
		PCGExFactories::PointFilters, false);
	GetInputFactories(
		Context, PCGExGraph::SourceFilterConnectables, Context->ConnetablesFiltersFactories,
		PCGExFactories::PointFilters, false);

	Context->CWStackingTolerance = FVector(1 / Settings->StackingPreventionTolerance);

	return true;
}

bool FPCGExConnectPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>>(
			[](const PCGExData::FPointIO* Entry) { return Entry->GetNum() >= 2; },
			[&](PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExConnectPoints
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(DistributedEdgesSet)

		PCGEX_DELETE(GraphBuilder)

		InPoints = nullptr;

		for (UPCGExProbeOperation* Op : ProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }
		for (UPCGExProbeOperation* Op : DirectProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }

		ProbeOperations.Empty();
		DirectProbeOperations.Empty();
		ChainProbeOperations.Empty();
		SharedProbeOperations.Empty();

		CachedTransforms.Empty();
		CanGenerate.Empty();

		PCGEX_DELETE(Octree)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConnectPoints::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectPoints)

		// TODO : Add Scoped Fetch

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointDataFacade->bSupportsDynamic = true;

		CWStackingTolerance = TypedContext->CWStackingTolerance;
		bPreventStacking = Settings->bPreventStacking;

		if (Settings->bProjectPoints)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			ProjectionDetails.Init(Context, PointDataFacade);
		}

		for (const UPCGExProbeFactoryBase* Factory : TypedContext->ProbeFactories)
		{
			UPCGExProbeOperation* NewOperation = Factory->CreateOperation();
			NewOperation->BindContext(Context);

			if (!NewOperation->PrepareForPoints(PointIO))
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

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderDetails, 2);
		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

		CanGenerate.SetNumUninitialized(PointIO->GetNum());
		CachedTransforms.SetNumUninitialized(PointIO->GetNum());

		if (!TypedContext->GeneratorsFiltersFactories.IsEmpty())
		{
			GeneratorsFilter = new PCGExPointFilter::TManager(PointDataFacade);
			GeneratorsFilter->Init(Context, TypedContext->GeneratorsFiltersFactories);
		}

		if (!TypedContext->ConnetablesFiltersFactories.IsEmpty())
		{
			ConnectableFilter = new PCGExPointFilter::TManager(PointDataFacade);
			ConnectableFilter->Init(Context, TypedContext->ConnetablesFiltersFactories);
		}

		bUseProjection = Settings->bProjectPoints;

		InPoints = &PointIO->GetIn()->GetPoints();

		if (!ProbeOperations.IsEmpty())
		{
			const FBox B = PointIO->GetIn()->GetBounds();
			Octree = new PositionOctree(bUseProjection ? ProjectionDetails.ProjectFlat(B.GetCenter()) : B.GetCenter(), B.GetExtent().Length());
		}
		else
		{
			if (GeneratorsFilter) { for (int i = 0; i < InPoints->Num(); i++) { CanGenerate[i] = GeneratorsFilter->Test(i); } }
			else { for (bool& Gen : CanGenerate) { Gen = true; } }
		}

		PCGExMT::FTaskGroup* PrepTask = AsyncManager->CreateGroup();
		PrepTask->SetOnCompleteCallback([&]() { OnPreparationComplete(); });
		PrepTask->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
			});

		PrepTask->PrepareRangesOnly(PointIO->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

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
				for (int i = 0; i < NumPoints; i++)
				{
					CachedTransforms[i] = ProjectionDetails.ProjectFlat(InPointsRef[i].Transform, i);

					CanGenerate[i] = GeneratorsFilter ? GeneratorsFilter->Test(i) : true;
					if (ConnectableFilter && ConnectableFilter->Test(i)) { continue; }
					Octree->AddElement(FPositionRef(i, FBoxSphereBounds(CachedTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
			else
			{
				for (int i = 0; i < NumPoints; i++)
				{
					CachedTransforms[i] = InPointsRef[i].Transform;

					CanGenerate[i] = GeneratorsFilter ? GeneratorsFilter->Test(i) : true;
					if (ConnectableFilter && !ConnectableFilter->Test(i)) { continue; }
					Octree->AddElement(FPositionRef(i, FBoxSphereBounds(CachedTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
		}

		PCGEX_DELETE(GeneratorsFilter)
		PCGEX_DELETE(ConnectableFilter)


		StartParallelLoopForPoints(PCGExData::ESource::In);
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<uint64>& Loops)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectPoints)

		FPointsProcessor::PrepareLoopScopesForPoints(Loops);
		for (int i = 0; i < Loops.Num(); i++) { DistributedEdgesSet.Add(new TSet<uint64>()); }
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::ProcessSinglePoint);

		if (!CanGenerate[Index]) { return; } // Not a generator

		TSet<uint64>* UniqueEdges = DistributedEdgesSet[LoopIdx];
		TSet<uint64>* LocalConnectionStack = nullptr;
		if (bPreventStacking) { LocalConnectionStack = new TSet<uint64>(); }

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
			for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->PrepareBestCandidate(Index, PointCopy, BestCandidates[i]); }
		}

		if (!ProbeOperations.IsEmpty())
		{
			double MaxRadius = TNumericLimits<double>::Min();
			if (!bUseVariableRadius) { MaxRadius = SharedSearchRadius; }
			else
			{
				for (UPCGExProbeOperation* Op : ProbeOperations) { MaxRadius = FMath::Max(MaxRadius, Op->SearchRadiusCache ? Op->SearchRadiusCache->Values[Index] : Op->SearchRadius); }
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
					bPreventStacking ? PCGEx::GH(Dir, CWStackingTolerance) : 0);

				if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->ProcessCandidateChained(i, PointCopy, EmplaceIndex, Candidates[EmplaceIndex], BestCandidates[i]); } }
			};

			Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(MaxRadius)), ProcessPoint);

			if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->ProcessBestCandidate(Index, PointCopy, BestCandidates[i], Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); } }

			if (!Candidates.IsEmpty())
			{
				Algo::Sort(Candidates, [&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, PointCopy, Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }
			}
			else
			{
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, PointCopy, Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }
			}
		}

		for (UPCGExProbeOperation* Op : DirectProbeOperations) { Op->ProcessNode(Index, PointCopy, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }

		PCGEX_DELETE(LocalConnectionStack)
	}

	void FProcessor::CompleteWork()
	{
		for (const TSet<uint64>* EdgesSet : DistributedEdgesSet)
		{
			GraphBuilder->Graph->InsertEdgesUnsafe(*EdgesSet, -1);
			delete EdgesSet;
		}

		DistributedEdgesSet.Empty();

		GraphBuilder->CompileAsync(AsyncManagerPtr);
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->Write();
		FPointsProcessor::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

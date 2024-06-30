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

FName UPCGExConnectPointsSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(ConnectPoints)

FPCGExConnectPointsContext::~FPCGExConnectPointsContext()
{
	PCGEX_TERMINATE_ASYNC

	ProbeFactories.Empty();
	GeneratorsFiltersFactories.Empty();
	ConnetablesFiltersFactories.Empty();

	PCGEX_DELETE(MainVtx);
}

bool FPCGExConnectPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (!PCGExFactories::GetInputFactories(Context, PCGExGraph::SourceProbesLabel, Context->ProbeFactories, {PCGExFactories::EType::Probe}, true))
	{
		return false;
	}

	PCGExFactories::GetInputFactories(Context, PCGExGraph::SourceFilterGenerators, Context->GeneratorsFiltersFactories, {PCGExFactories::EType::FilterPoint}, false);
	PCGExFactories::GetInputFactories(Context, PCGExGraph::SourceFilterConnectables, Context->ConnetablesFiltersFactories, {PCGExFactories::EType::FilterPoint}, false);

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

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExConnectPoints
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(DistributedEdgesSet)

		PCGEX_DELETE(GraphBuilder)

		StartPtr = nullptr;
		InPoints = nullptr;

		for (UPCGExProbeOperation* Op : ProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }
		for (UPCGExProbeOperation* Op : DirectProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }

		ProbeOperations.Empty();
		DirectProbeOperations.Empty();
		ChainProbeOperations.Empty();
		SharedProbeOperations.Empty();

		Positions.Empty();
		CanGenerate.Empty();

		Octree = nullptr;
		PCGEX_DELETE(LocalOctree)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectPoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

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

			if (NewOperation->SearchRadiusSquared == -1) { bUseVariableRadius = true; }
			MaxRadiusSquared = FMath::Max(NewOperation->SearchRadiusSquared, MaxRadiusSquared);

			if (NewOperation->RequiresChainProcessing()) { ChainProbeOperations.Add(NewOperation); }
			else { SharedProbeOperations.Add(NewOperation); }

			ProbeOperations.Add(NewOperation);
		}

		NumChainedOps = ChainProbeOperations.Num();

		if (ProbeOperations.IsEmpty() && DirectProbeOperations.IsEmpty()) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderSettings, 2);
		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

		InPoints = &PointIO->GetIn()->GetPoints();

		CanGenerate.SetNumUninitialized(InPoints->Num());

		PCGExPointFilter::TManager* GeneratorsFilter = nullptr;
		if (!TypedContext->GeneratorsFiltersFactories.IsEmpty())
		{
			GeneratorsFilter = new PCGExPointFilter::TManager(PointDataCache);
			GeneratorsFilter->Init(Context, TypedContext->GeneratorsFiltersFactories);
		}

		PCGExPointFilter::TManager* ConnectableFilter = nullptr;
		if (!TypedContext->ConnetablesFiltersFactories.IsEmpty())
		{
			ConnectableFilter = new PCGExPointFilter::TManager(PointDataCache);
			ConnectableFilter->Init(Context, TypedContext->ConnetablesFiltersFactories);

			if (!ProbeOperations.IsEmpty())
			{
				const FBox B = PointIO->GetIn()->GetBounds();
				LocalOctree = new UPCGPointData::PointOctree(B.GetCenter(), B.GetExtent().Length());

				int Index = 0;
				for (const FPCGPoint& Point : (*InPoints))
				{
					if (!ConnectableFilter->Test(Index++)) { continue; }
					LocalOctree->AddElement(FPCGPointRef(Point));
				}

				Octree = LocalOctree;
			}
		}

		if (!ProbeOperations.IsEmpty())
		{
			if (!Octree) { Octree = &PointIO->GetIn()->GetOctree(); }
			StartPtr = InPoints->GetData();

			const TArray<FPCGPoint>& InPointsRef = (*InPoints);
			Positions.SetNum(InPointsRef.Num());

			for (int i = 0; i < InPointsRef.Num(); i++) { Positions[i] = InPointsRef[i].Transform.GetLocation(); }
		}

		for (int i = 0; i < CanGenerate.Num(); i++) { CanGenerate[i] = GeneratorsFilter ? GeneratorsFilter->Test(i) : true; }

		PCGEX_DELETE(GeneratorsFilter)
		PCGEX_DELETE(ConnectableFilter)

		CWStackingTolerance = TypedContext->CWStackingTolerance;
		bPreventStacking = Settings->bPreventStacking;

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareParallelLoopForPoints(const TArray<uint64>& Loops)
	{
		FPointsProcessor::PrepareParallelLoopForPoints(Loops);
		for (int i = 0; i < Loops.Num(); i++) { DistributedEdgesSet.Add(new TSet<uint64>()); }
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::ProcessSinglePoint);

		if (!CanGenerate[Index]) { return; } // Not a generator

		TSet<uint64>* UniqueEdges = DistributedEdgesSet[LoopIdx];
		TSet<uint64>* LocalConnectionStack = nullptr;
		if (bPreventStacking) { LocalConnectionStack = new TSet<uint64>(); }


		TArray<PCGExProbing::FBestCandidate> BestCandidates;

		if (NumChainedOps > 0)
		{
			BestCandidates.SetNum(NumChainedOps);
			for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->PrepareBestCandidate(Index, Point, BestCandidates[i]); }
		}

		if (!ProbeOperations.IsEmpty())
		{
			double MaxRadius = TNumericLimits<double>::Min();
			if (!bUseVariableRadius) { MaxRadius = MaxRadiusSquared; }
			else { for (UPCGExProbeOperation* Op : ProbeOperations) { MaxRadius = FMath::Max(MaxRadius, Op->SearchRadiusCache ? Op->SearchRadiusCache->Values[Index] : Op->SearchRadiusSquared); } }

			const FVector Origin = Positions[Index];

			TArray<PCGExProbing::FCandidate> Candidates;

			auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t OtherPointIndex = InPointRef.Point - StartPtr;
				if (static_cast<int32>(OtherPointIndex) == Index) { return; }

				const FVector Position = Positions[OtherPointIndex];
				const FVector Dir = (Origin - Position).GetSafeNormal();
				const int32 EmplaceIndex = Candidates.Emplace(
					OtherPointIndex,
					Dir,
					FVector::DistSquared(Position, Origin),
					bPreventStacking ? PCGEx::GH(Dir, CWStackingTolerance) : 0);

				if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->ProcessCandidateChained(i, Point, EmplaceIndex, Candidates[EmplaceIndex], BestCandidates[i]); } }
			};

			Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(MaxRadius))), ProcessPoint);

			if (NumChainedOps > 0) { for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->ProcessBestCandidate(Index, Point, BestCandidates[i], Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); } }

			if (!Candidates.IsEmpty())
			{
				Algo::Sort(Candidates, [&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, Point, Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }
			}
			else
			{
				for (UPCGExProbeOperation* Op : SharedProbeOperations) { Op->ProcessCandidates(Index, Point, Candidates, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }
			}
		}

		for (UPCGExProbeOperation* Op : DirectProbeOperations) { Op->ProcessNode(Index, Point, LocalConnectionStack, CWStackingTolerance, UniqueEdges); }

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

		GraphBuilder->Write(Context);
		FPointsProcessor::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

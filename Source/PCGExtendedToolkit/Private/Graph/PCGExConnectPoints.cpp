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

	PCGExFactories::GetInputFactories(Context, PCGExGraph::SourceFilterGenerators, Context->GeneratorsFiltersFactories, {PCGExFactories::EType::Filter}, false);
	PCGExFactories::GetInputFactories(Context, PCGExGraph::SourceFilterConnectables, Context->ConnetablesFiltersFactories, {PCGExFactories::EType::Filter}, false);

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
		PCGEX_DELETE(GraphBuilder)

		StartPtr = nullptr;
		InPoints = nullptr;

		for (UPCGExProbeOperation* Op : ProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }
		for (UPCGExProbeOperation* Op : DirectProbeOperations) { PCGEX_DELETE_UOBJECT(Op) }

		ProbeOperations.Empty();
		DirectProbeOperations.Empty();

		Positions.Empty();
		PointStatus.Empty();
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

			ProbeOperations.Add(NewOperation);
		}

		if (ProbeOperations.IsEmpty() && DirectProbeOperations.IsEmpty()) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderSettings, 2);
		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

		InPoints = &PointIO->GetIn()->GetPoints();

		if (!ProbeOperations.IsEmpty())
		{
			Octree = &PointIO->GetIn()->GetOctree();
			StartPtr = InPoints->GetData();

			const TArray<FPCGPoint>& InPointsRef = (*InPoints);
			Positions.SetNum(InPointsRef.Num());

			for (int i = 0; i < InPointsRef.Num(); i++) { Positions[i] = InPointsRef[i].Transform.GetLocation(); }
		}

		PointStatus.SetNumUninitialized(InPoints->Num());

		PCGExDataFilter::TEarlyExitFilterManager* GeneratorsFilter = nullptr;
		if (!TypedContext->GeneratorsFiltersFactories.IsEmpty())
		{
			GeneratorsFilter = new PCGExDataFilter::TEarlyExitFilterManager(PointDataCache);
			GeneratorsFilter->Register<UPCGExFilterFactoryBase>(Context, TypedContext->GeneratorsFiltersFactories);
			GeneratorsFilter->bCacheResults = false;
		}

		PCGExDataFilter::TEarlyExitFilterManager* ConnectableFilter = nullptr;
		if (!TypedContext->ConnetablesFiltersFactories.IsEmpty())
		{
			ConnectableFilter = new PCGExDataFilter::TEarlyExitFilterManager(PointDataCache);
			ConnectableFilter->Register<UPCGExFilterFactoryBase>(Context, TypedContext->ConnetablesFiltersFactories);
			ConnectableFilter->bCacheResults = false;
		}

		for (int i = 0; i < PointStatus.Num(); i++)
		{
			uint8 Status = 0;
			Status |= (GeneratorsFilter ? GeneratorsFilter->DirectTest(i) : true) ? 1 : 0;
			Status |= (ConnectableFilter ? ConnectableFilter->DirectTest(i) : true) ? 2 : 0;
			PointStatus[i] = Status;
		}

		PCGEX_DELETE(GeneratorsFilter)
		PCGEX_DELETE(ConnectableFilter)

		CWStackingTolerance = TypedContext->CWStackingTolerance;
		bPreventStacking = Settings->bPreventStacking;

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectPointsElement::ProcessSinglePoint);

		if ((PointStatus[Index] & 1) == 0) { return; } // Not a generator

		TSet<uint64>* TempStack = nullptr;
		if (bPreventStacking) { TempStack = new TSet<uint64>(); }

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
				if (static_cast<int32>(OtherPointIndex) == Index ||        // Is currently sampled point
					((PointStatus[OtherPointIndex] & 2) == 0)) { return; } // Is not accepting connections

				const FVector Position = Positions[OtherPointIndex];
				const FVector Dir = (Origin - Position).GetSafeNormal();
				Candidates.Emplace(
					OtherPointIndex,
					Dir,
					FVector::DistSquared(Position, Origin),
					bPreventStacking ? PCGEx::GH(Dir, CWStackingTolerance) : 0);
			};

			Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(MaxRadius))), ProcessPoint);

			if (!Candidates.IsEmpty())
			{
				//Candidates.Sort([&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
				Algo::Sort(Candidates, [&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
				for (UPCGExProbeOperation* Op : ProbeOperations) { Op->ProcessCandidates(Index, Point, Candidates, TempStack, CWStackingTolerance); }
			}
		}

		for (UPCGExProbeOperation* Op : DirectProbeOperations) { Op->ProcessNode(Index, Point, TempStack, CWStackingTolerance); }
		
		PCGEX_DELETE(TempStack)
	}

	void FProcessor::CompleteWork()
	{
		auto InsertEdges = [&](UPCGExProbeOperation* Op)
		{
			GraphBuilder->Graph->InsertEdgesUnsafe(Op->UniqueEdges, -1);
			Op->UniqueEdges.Empty();
		};

		for (UPCGExProbeOperation* Op : ProbeOperations) { InsertEdges(Op); }
		for (UPCGExProbeOperation* Op : DirectProbeOperations) { InsertEdges(Op); }

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

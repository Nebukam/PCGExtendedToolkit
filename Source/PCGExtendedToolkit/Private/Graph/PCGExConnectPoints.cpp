// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConnectPoints.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionHelpers.h"
#include "Graph/Probes/PCGExProbeFactoryProvider.h"
#include "Graph/Probes/PCGExProbeOperation.h"
#include "Graph/Probes/PCGExProbing.h"

#define LOCTEXT_NAMESPACE "PCGExConnectPointsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceProbesLabel, "Probes used to connect points", Required, {})

	PCGEX_PIN_FACTORIES(PCGExGraph::SourceFilterGenerators, "Points that don't meet requirements won't generate connections", Normal, {})
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceFilterConnectables, "Points that don't meet requirements can't receive connections", Normal, {})

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExConnectPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ConnectPoints)

bool FPCGExConnectPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectPoints)

	if (!PCGExFactories::GetInputFactories<UPCGExProbeFactoryData>(
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
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExConnectPoints::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters. Make sure inputs have at least 2 points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExConnectPoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
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

		for (const UPCGExProbeFactoryData* Factory : Context->ProbeFactories)
		{
			TSharedPtr<FPCGExProbeOperation> NewOperation = Factory->CreateOperation(Context);
			NewOperation->BindContext(ExecutionContext);
			NewOperation->PrimaryDataFacade = PointDataFacade;

			if (!NewOperation->PrepareForPoints(PointDataFacade->Source)) { continue; }

			if (!NewOperation->RequiresOctree())
			{
				DirectProbes.Add(NewOperation);
				continue;
			}

			if (!NewOperation->SearchRadius->IsConstant()) { bUseVariableRadius = true; }
			SharedSearchRadius = FMath::Max(SharedSearchRadius, NewOperation->BaseConfig->SearchRadiusConstant);

			if (NewOperation->RequiresChainProcessing()) { ChainProbeOperations.Add(NewOperation); }
			else { SharedProbeOperations.Add(NewOperation); }

			SearchProbes.Add(NewOperation);
		}

		NumChainedOps = ChainProbeOperations.Num();

		if (SearchProbes.IsEmpty() && DirectProbes.IsEmpty()) { return false; }

		if (!PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New)) { return false; }
		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);

		CanGenerate.SetNumUninitialized(NumPoints);
		AcceptConnections.SetNumUninitialized(NumPoints);

		PCGEx::InitArray(WorkingTransforms, NumPoints);

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

		if (!SearchProbes.IsEmpty())
		{
			// If we have search probes, build the octree
			const FBox B = PointDataFacade->GetIn()->GetBounds();
			Octree = MakeUnique<PCGEx::FIndexedItemOctree>(bUseProjection ? ProjectionDetails.ProjectFlat(B.GetCenter()) : B.GetCenter(), B.GetExtent().Length());
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, PrepTask)

		PrepTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnPreparationComplete();
			};

		PrepTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->Fetch(Scope);
				PCGEX_SCOPE_LOOP(i)
				{
					This->CanGenerate[i] = This->GeneratorsFilter ? This->GeneratorsFilter->Test(i) : true;
					This->AcceptConnections[i] = This->ConnectableFilter ? This->ConnectableFilter->Test(i) : true;
				}
			};

		PrepTask->StartSubLoops(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::OnPreparationComplete()
	{

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();		
		const TConstPCGValueRange<FTransform> OriginalTransforms = InPointData->GetConstTransformValueRange();
		
		const int32 NumPoints = OriginalTransforms.Num();

		if (!SearchProbes.IsEmpty())
		{
			constexpr double PPRefRadius = 0.05;
			const FVector PPRefExtents = FVector(PPRefRadius);

			if (bUseProjection)
			{
				for (int i = 0; i < NumPoints; i++)
				{
					WorkingTransforms[i] = ProjectionDetails.ProjectFlat(OriginalTransforms[i], i);
					if (!AcceptConnections[i]) { continue; }
					Octree->AddElement(PCGEx::FIndexedItem(i, FBoxSphereBounds(WorkingTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
			else
			{
				for (int i = 0; i < NumPoints; i++)
				{
					WorkingTransforms[i] = OriginalTransforms[i];
					if (!AcceptConnections[i]) { continue; }
					Octree->AddElement(PCGEx::FIndexedItem(i, FBoxSphereBounds(WorkingTransforms[i].GetLocation(), PPRefExtents, PPRefRadius)));
				}
			}
		}

		GeneratorsFilter.Reset();
		ConnectableFilter.Reset();

		StartParallelLoopForPoints(PCGExData::EIOSide::In);
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		FPointsProcessor::PrepareLoopScopesForPoints(Loops);
		ScopedEdges = MakeShared<PCGExMT::TScopedSet<uint64>>(Loops, 10);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ConnectPoints::ProcessPoints);
		
		PointDataFacade->Fetch(Scope);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		const TConstPCGValueRange<FTransform> OriginalTransforms = InPointData->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			
			if (!CanGenerate[Index]) { continue; } // Not a generator

			TSet<uint64>* UniqueEdges = ScopedEdges->Get(Scope).Get();
			TUniquePtr<TSet<FInt32Vector>> LocalCoincidence;
			if (bPreventCoincidence) { LocalCoincidence = MakeUnique<TSet<FInt32Vector>>(); }

			const FTransform& CandidateTransform = bUseProjection ? WorkingTransforms[Index] : OriginalTransforms[Index];

			TArray<PCGExProbing::FBestCandidate> BestCandidates;

			if (NumChainedOps > 0)
			{
				BestCandidates.SetNum(NumChainedOps);
				for (int i = 0; i < NumChainedOps; i++) { ChainProbeOperations[i]->PrepareBestCandidate(Index, CandidateTransform, BestCandidates[i]); }
			}

			if (!SearchProbes.IsEmpty())
			{
				double MaxRadius = 0;
				if (!bUseVariableRadius) { MaxRadius = SharedSearchRadius; }
				else
				{
					for (const TSharedPtr<FPCGExProbeOperation>& Op : SearchProbes) { MaxRadius = FMath::Max(MaxRadius, Op->GetSearchRadius(Index)); }
				}

				const FVector Origin = WorkingTransforms[Index].GetLocation();

				TArray<PCGExProbing::FCandidate> Candidates;

				auto ProcessPoint = [&](const PCGEx::FIndexedItem& InPositionRef)
				{
					const int32 OtherPointIndex = InPositionRef.Index;
					if (OtherPointIndex == Index) { return; }

					const FVector Position = WorkingTransforms[OtherPointIndex].GetLocation();
					const FVector Dir = (Origin - Position).GetSafeNormal();
					const int32 EmplaceIndex = Candidates.Emplace(
						OtherPointIndex,
						Dir,
						FVector::DistSquared(Position, Origin),
						bPreventCoincidence ? PCGEx::I323(Dir, CWCoincidenceTolerance) : FInt32Vector::ZeroValue);

					if (NumChainedOps > 0)
					{
						for (int i = 0; i < NumChainedOps; i++)
						{
							ChainProbeOperations[i]->ProcessCandidateChained(i, CandidateTransform, EmplaceIndex, Candidates[EmplaceIndex], BestCandidates[i]);
						}
					}
				};

				Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(MaxRadius)), ProcessPoint);

				if (NumChainedOps > 0)
				{
					for (int i = 0; i < NumChainedOps; i++)
					{
						ChainProbeOperations[i]->ProcessBestCandidate(Index, CandidateTransform, BestCandidates[i], Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges);
					}
				}

				if (!Candidates.IsEmpty())
				{
					Algo::Sort(Candidates, [&](const PCGExProbing::FCandidate& A, const PCGExProbing::FCandidate& B) { return A.Distance < B.Distance; });
					for (const TSharedPtr<FPCGExProbeOperation>& Op : SharedProbeOperations)
					{
						Op->ProcessCandidates(Index, CandidateTransform, Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges);
					}
				}
				else
				{
					for (const TSharedPtr<FPCGExProbeOperation>& Op : SharedProbeOperations)
					{
						Op->ProcessCandidates(Index, CandidateTransform, Candidates, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges);
					}
				}
			}

			for (const TSharedPtr<FPCGExProbeOperation>& Op : DirectProbes)
			{
				Op->ProcessNode(Index, CandidateTransform, LocalCoincidence.Get(), CWCoincidenceTolerance, UniqueEdges, AcceptConnections);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		ScopedEdges->ForEach([this](const TSet<uint64>& Edges) { GraphBuilder->Graph->InsertEdges_Unsafe(Edges, -1); });
		ScopedEdges.Reset();

		GraphBuilder->CompileAsync(AsyncManager, false);
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		PointDataFacade->Write(AsyncManager);
		GraphBuilder->StageEdgesOutputs();
	}

	void FProcessor::Cleanup()
	{
		TPointsProcessor<FPCGExConnectPointsContext, UPCGExConnectPointsSettings>::Cleanup();

		SearchProbes.Empty();
		DirectProbes.Empty();
		ChainProbeOperations.Empty();
		SharedProbeOperations.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExCutEdges.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCutEdges"
#define PCGEX_NAMESPACE CutEdges

TArray<FPCGPinProperties> UPCGExCutEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGExGraph::SourcePathsLabel, "Cutting paths.", Required, {})
	if (NodeHandling == EPCGExCutEdgesNodeHandlingMode::Remove)
	{
		PCGEX_PIN_PARAMS(PCGExCutEdges::SourceNodeFilters, "Node preservation filters.", Normal, {})
	}
	PCGEX_PIN_PARAMS(PCGExCutEdges::SourceEdgeFilters, "Edge preservation filters.", Normal, {})

	return PinProperties;
}

PCGExData::EInit UPCGExCutEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExCutEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(CutEdges)

bool FPCGExCutEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CutEdges)

	PCGEX_FWD(IntersectionDetails)
	Context->IntersectionDetails.Init();

	PCGEX_FWD(GraphBuilderDetails)

	GetInputFactories(Context, PCGExCutEdges::SourceEdgeFilters, Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	if (Settings->NodeHandling == EPCGExCutEdgesNodeHandlingMode::Remove)
	{
		GetInputFactories(Context, PCGExCutEdges::SourceNodeFilters, Context->NodeFilterFactories, PCGExFactories::ClusterNodeFilters, false);
	}

	TSharedPtr<PCGExData::FPointIOCollection> PathCollection = MakeShared<PCGExData::FPointIOCollection>(Context, PCGExGraph::SourcePathsLabel);
	if (PathCollection->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Empty paths."));
		return false;
	}

	Context->PathFacades.Reserve(PathCollection->Num());
	Context->Paths.Reserve(PathCollection->Num());

	int32 ExcludedNum = 0;

	Context->PathsAttributesInfos = MakeShared<PCGEx::FAttributesInfos>();

	TSet<FName> TypeMismatches;
	for (TSharedPtr<PCGExData::FPointIO> PathIO : PathCollection->Pairs)
	{
		if (PathIO->GetNum() < 2)
		{
			ExcludedNum++;
			continue;
		}

		TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());
		Facade->bSupportsScopedGet = Context->bScopedAttributeGet;

		TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(Facade->GetIn()->Metadata);
		Context->PathsAttributesInfos->Append(AttributesInfos, TypeMismatches);

		Context->PathFacades.Add(Facade.ToSharedRef());
	}

	if (ExcludedNum != 0)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input paths had less than 2 points and will be ignored."));
	}

	if (Context->PathFacades.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No valid paths found."));
		return false;
	}

	PCGEX_FWD(ClosedLoop)
	Context->ClosedLoop.Init();

	if (Settings->Mode == EPCGExCutEdgesMode::Crossings)
	{
		if (Settings->IntersectionDetails.bWriteCrossing) { PCGEX_VALIDATE_NAME(Settings->IntersectionDetails.CrossingAttributeName) }
		PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation)
		Context->CrossingBlending = Settings->CrossingBlending;
	}


	return true;
}

bool FPCGExCutEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCutEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CutEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGExPaths::State_BuildingPaths);
		PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), BuildPathsTask)

		BuildPathsTask->OnIterationRangeStartCallback = [Settings, Context](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			TSharedRef<PCGExData::FFacade> PathFacade = Context->PathFacades[StartIndex];
			TSharedPtr<PCGExPaths::FPath> Path = PCGExPaths::MakePath(
				PathFacade->Source->GetIn()->GetPoints(), 0,
				Context->ClosedLoop.IsClosedLoop(PathFacade->Source), false);

			Path->BuildEdgeOctree();

			Context->Paths.Add(Path.ToSharedRef());

			if (Settings->Mode != EPCGExCutEdgesMode::Crossings) { return; }


			TSharedPtr<PCGExData::FFacadePreloader> Preloader = MakeShared<PCGExData::FFacadePreloader>();
			Context->PathsPreloaders.Add(Preloader);

			// TODO : Preload path data here once cross blending is implemented

			Preloader->StartLoading(Context->GetAsyncManager(), PathFacade);
		};

		BuildPathsTask->StartRangePrepareOnly(Context->PathFacades.Num(), 1);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExPaths::State_BuildingPaths)
	{
		if (!Context->StartProcessingClusters<PCGExCutEdges::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExCutEdges::FProcessorBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(Settings->Mode == EPCGExCutEdgesMode::Crossings ? PCGEx::State_Done : PCGExGraph::State_ReadyToCompile)
	if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }

	// TODO : Process cross blending once graph is compiled?

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExCutEdges
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		return MakeShared<PCGExCluster::FCluster>(InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, false, true, false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCutEdges::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		EdgeFilterCache.Init(false, EdgeDataFacade->Source->GetNum());

		if (!Context->EdgeFilterFactories.IsEmpty())
		{
			EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
			EdgeFilterManager->bUseEdgeAsPrimary = true;
			if (!EdgeFilterManager->Init(ExecutionContext, Context->EdgeFilterFactories)) { return false; }
		}

		if (!Context->NodeFilterFactories.IsEmpty())
		{
			NodeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
			if (!NodeFilterManager->Init(ExecutionContext, Context->NodeFilterFactories)) { return false; }
		}

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
	{
		const int32 MaxIndex = StartIndex + Count;

		EdgeDataFacade->Fetch(StartIndex, Count);

		TArray<PCGExGraph::FIndexedEdge>& Edges = *Cluster->Edges;
		if (EdgeFilterManager) { for (int i = StartIndex; i < MaxIndex; i++) { EdgeFilterCache[i] = EdgeFilterManager->Test(Edges[i]); } }
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		if (EdgeFilterCache[EdgeIndex]) { return; }

		const FVector A1 = EdgeDataFacade->Source->GetInPoint(Edge.Start).Transform.GetLocation();
		const FVector B1 = EdgeDataFacade->Source->GetInPoint(Edge.End).Transform.GetLocation();
		const FVector Dir = (B1 - A1).GetSafeNormal();

		FBox EdgeBox = FBox(ForceInit);
		EdgeBox += A1;
		EdgeBox += B1;

		if (Settings->Mode == EPCGExCutEdgesMode::Cut)
		{
			for (TSharedPtr<PCGExPaths::FPath> Path : Context->Paths)
			{
				if (!Path->Bounds.Intersect(EdgeBox)) { continue; }

				// Check paths
				Path->GetEdgeOctree()->FindFirstElementWithBoundsTest(
					EdgeBox, [&](const PCGExPaths::FPathEdge* PathEdge)
					{
						if (!Edge.bValid) { return false; }

						if (Context->IntersectionDetails.bUseMinAngle || Context->IntersectionDetails.bUseMaxAngle)
						{
							if (!Context->IntersectionDetails.CheckDot(FMath::Abs(FVector::DotProduct(Path->GetEdgeDir(*PathEdge), Dir))))
							{
								return true;
							}
						}

						const FVector A2 = Path->GetPosUnsafe(PathEdge->Start);
						const FVector B2 = Path->GetPosUnsafe(PathEdge->End);
						FVector A = FVector::ZeroVector;
						FVector B = FVector::ZeroVector;

						FMath::SegmentDistToSegment(A1, B1, A2, B2, A, B);
						if (A == A1 || A == B1 || B == A2 || B == B2) { return true; }
						
						if (FVector::DistSquared(A, B) >= Context->IntersectionDetails.ToleranceSquared) { return true; }

						FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
						return false;
					});

				if (!Edge.bValid) { return; }
			}
		}
		else
		{
			/*
			for (TSharedPtr<PCGExPaths::FPath> Path : Context->Paths)
			{
				if (!Path->Bounds.Intersect(EdgeBox)) { continue; }

				Path->GetEdgeOctree()->FindFirstElementWithBoundsTest(
					EdgeBox, [&](PCGExPaths::FPathEdge* PathEdge)
					{
						if (!Edge.bValid) { return false; }

						if (Context->IntersectionDetails.bUseMinAngle || Context->IntersectionDetails.bUseMaxAngle)
						{
							if (!Context->IntersectionDetails.CheckDot(FMath::Abs(FVector::DotProduct(Dir, Path->GetEdgeDir(*PathEdge))))) { return true; }
						}

						const FVector A2 = Path->GetPosUnsafe(PathEdge->Start);
						const FVector B2 = Path->GetPosUnsafe(PathEdge->End);
						FVector AB1 = FVector::ZeroVector;
						FVector AB2 = FVector::ZeroVector;

						FMath::SegmentDistToSegment(A1, B1, A2, B2, AB1, AB2);

						if (FVector::DistSquared(AB1, AB2) >= Context->IntersectionDetails.ToleranceSquared) { return true; }

						// TODO : Register crossing
						
						return true;
					});

				if (!Edge.bValid) { return; }
			}
			*/
		}
	}

	void FProcessor::CompleteWork()
	{
		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);

		if (ValidEdges.IsEmpty()) { return; }

		GraphBuilder->Graph->InsertEdges(ValidEdges);
	}

	void FProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)

		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->NodeFilterFactories, FacadePreloader);
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

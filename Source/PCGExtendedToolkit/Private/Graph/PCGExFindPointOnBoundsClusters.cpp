// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindPointOnBoundsClusters.h"

#include "Data/PCGExPointIOMerger.h"

#define LOCTEXT_NAMESPACE "PCGExFindPointOnBoundsClusters"
#define PCGEX_NAMESPACE FindPointOnBoundsClusters

PCGExData::EIOInit UPCGExFindPointOnBoundsClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }
PCGExData::EIOInit UPCGExFindPointOnBoundsClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

void FPCGExFindPointOnBoundsClustersContext::ClusterProcessing_InitialProcessingDone()
{
	FPCGExEdgesProcessorContext::ClusterProcessing_InitialProcessingDone();
	PCGEX_SETTINGS_LOCAL(FindPointOnBoundsClusters)
}

PCGEX_INITIALIZE_ELEMENT(FindPointOnBoundsClusters)

TArray<FPCGPinProperties> UPCGExFindPointOnBoundsClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

bool FPCGExFindPointOnBoundsClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }
	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBoundsClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		TSharedPtr<PCGExData::FPointIOCollection> Collection = Settings->SearchMode == EPCGExClusterClosestSearchMode::Node ? Context->MainPoints : Context->MainEdges;
		TSet<FName> AttributeMismatches;

		Context->BestIndices.Init(-1, Context->MainEdges->Num());
		Context->IOMergeSources.Init(nullptr, Context->MainEdges->Num());

		Context->MergedOut = PCGExData::NewPointIO(Context, Settings->GetMainOutputLabel(), 0);
		Context->MergedAttributesInfos = PCGEx::FAttributesInfos::Get(Collection, AttributeMismatches);

		Context->CarryOverDetails.Attributes.Prune(*Context->MergedAttributesInfos);
		Context->CarryOverDetails.Attributes.Prune(AttributeMismatches);

		Context->MergedOut->InitializeOutput(PCGExData::EIOInit::NewOutput);
		Context->MergedOut->GetOut()->GetMutablePoints().SetNum(Context->MainEdges->Num());
		Context->MergedOut->GetOutKeys(true);

		if (!AttributeMismatches.IsEmpty() && !Settings->bQuietAttributeMismatchWarning)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some attributes on incoming data share the same name but not the same type. Whatever type was discovered first will be used."));
		}
	}

	return true;
}

bool FPCGExFindPointOnBoundsClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindPointOnBoundsClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBoundsClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExFindPointOnBoundsClusters::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExFindPointOnBoundsClusters::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		TSharedPtr<PCGExData::FPointIOCollection> Collection = Settings->SearchMode == EPCGExClusterClosestSearchMode::Node ? Context->MainPoints : Context->MainEdges;
		PCGExFindPointOnBounds::MergeBestCandidatesAttributes(
			Context->MergedOut,
			Context->IOMergeSources,
			Context->BestIndices,
			*Context->MergedAttributesInfos);

		Context->MergedOut->StageOutput();
	}
	else
	{
		if (Settings->SearchMode == EPCGExClusterClosestSearchMode::Node) { Context->MainPoints->StageOutputs(); }
		else { Context->MainEdges->StageOutputs(); }
	}

	return Context->TryComplete();
}


namespace PCGExFindPointOnBoundsClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		const FVector E = Cluster->Bounds.GetExtent();
		SearchPosition = Cluster->Bounds.GetCenter() + Cluster->Bounds.GetExtent() * Settings->UVW;
		Cluster->RebuildOctree(Settings->SearchMode);

		if (Settings->SearchMode == EPCGExClusterClosestSearchMode::Node) { StartParallelLoopForNodes(); }
		else { StartParallelLoopForEdges(); }

		return true;
	}

	void FProcessor::UpdateCandidate(const FVector& InPosition, const int32 InIndex)
	{
		if (const double Dist = FVector::Dist(InPosition, SearchPosition); Dist < BestDistance)
		{
			FWriteScopeLock WriteLock(BestIndexLock);
			BestPosition = InPosition;
			BestIndex = InIndex;
			BestDistance = Dist;
		}
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		UpdateCandidate(Cluster->GetPos(Node), Node.PointIndex);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		UpdateCandidate(Cluster->GetClosestPointOnEdge(EdgeIndex, SearchPosition), EdgeIndex);
	}

	void FProcessor::CompleteWork()
	{
		const TSharedPtr<PCGExData::FPointIO> IORef = Settings->SearchMode == EPCGExClusterClosestSearchMode::Node ? VtxDataFacade->Source : EdgeDataFacade->Source;

		const FVector Offset = (BestPosition - Cluster->Bounds.GetCenter()).GetSafeNormal() * Settings->Offset;

		if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
		{
			const int32 SourceIndex = EdgeDataFacade->Source->IOIndex;
			Context->IOMergeSources[SourceIndex] = IORef;

			const PCGMetadataEntryKey OriginalKey = Context->MergedOut->GetOut()->GetMutablePoints()[SourceIndex].MetadataEntry;
			FPCGPoint& OutPoint = (Context->MergedOut->GetOut()->GetMutablePoints()[SourceIndex] = IORef->GetInPoint(BestIndex));
			OutPoint.MetadataEntry = OriginalKey;
			OutPoint.Transform.AddToTranslation(Offset);
		}
		else
		{
			IORef->InitializeOutput(PCGExData::EIOInit::NewOutput);
			IORef->GetOut()->GetMutablePoints().SetNum(1);

			FPCGPoint& OutPoint = (IORef->GetOut()->GetMutablePoints()[0] = IORef->GetInPoint(BestIndex));
			IORef->GetOut()->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			OutPoint.Transform.AddToTranslation(Offset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

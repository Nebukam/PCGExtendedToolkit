// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSubGraph.h"

#include "Core/PCGExContext.h"
#include "PCGExRandom.h"
#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSortHelpers.h"
#include "Details/PCGExDetailsIntersection.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Metadata/PCGMetadata.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#include "Async/ParallelFor.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExUnionData.h"
#include "Details/PCGExDetailsDistances.h"

namespace PCGExGraph
{
	void FSubGraph::Add(const FEdge& Edge)
	{
		Edges.Emplace(Edge.Index, Edge.H64U());
		if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
	}

	void FSubGraph::Shrink()
	{
		Nodes.Shrink();
		Edges.Shrink();
	}

	void FSubGraph::BuildCluster(const TSharedRef<PCGExCluster::FCluster>& InCluster)
	{
		// Correct edge IO Index that has been overwritten during subgraph processing
		PCGEX_PARALLEL_FOR(FlattenedEdges.Num(), FlattenedEdges[i].IOIndex = -1;)
		InCluster->BuildFrom(SharedThis(this));

		// Look into the cost of this

		//if (TaskManager) { NewCluster->ExpandEdges(TaskManager); }
		//else { NewCluster->GetExpandedEdges(true); }
	}

	int32 FSubGraph::GetFirstInIOIndex()
	{
		for (const int32 InIOIndex : EdgesInIOIndices) { return InIOIndex; }
		return -1;
	}

	void FSubGraph::Compile(const TWeakPtr<PCGExMT::IAsyncHandleGroup>& InParentHandle, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<FGraphBuilder>& InBuilder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::ExecuteTask);

		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();
		const TArray<FNode>& ParentGraphNodes = ParentGraph->Nodes;
		const TArray<FEdge>& ParentGraphEdges = ParentGraph->Edges;

		WeakBuilder = InBuilder;
		WeakTaskManager = TaskManager;

		const int32 NumEdges = Edges.Num();
		PCGEx::RadixSort(Edges);

		FlattenedEdges.SetNumUninitialized(NumEdges);

		const UPCGBasePointData* InEdgeData = EdgesDataFacade->GetIn();

		EPCGPointNativeProperties AllocateProperties = InEdgeData ? InEdgeData->GetAllocatedProperties() : EPCGPointNativeProperties::None;
		AllocateProperties |= EPCGPointNativeProperties::MetadataEntry;

		if (InBuilder->OutputDetails->bWriteEdgePosition)
		{
			AllocateProperties |= EPCGPointNativeProperties::Transform;
		}

		if (InBuilder->OutputDetails->BasicEdgeSolidification.SolidificationAxis != EPCGExMinimalAxis::None)
		{
			AllocateProperties |= EPCGPointNativeProperties::Transform;
			AllocateProperties |= EPCGPointNativeProperties::BoundsMin;
			AllocateProperties |= EPCGPointNativeProperties::BoundsMax;
		}

		if (ParentGraph->bRefreshEdgeSeed || InBuilder->OutputDetails->bRefreshEdgeSeed)
		{
			AllocateProperties |= EPCGPointNativeProperties::Seed;
		}

		UPCGBasePointData* OutEdgeData = EdgesDataFacade->GetOut();
		(void)PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutEdgeData, NumEdges, AllocateProperties);
		EnumRemoveFlags(AllocateProperties, EPCGPointNativeProperties::MetadataEntry);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::BuildEdgesEntries);

			const TPCGValueRange<int64> OutMetadataEntries = OutEdgeData->GetMetadataEntryValueRange(false);
			UPCGMetadata* Metadata = OutEdgeData->MutableMetadata();

			TArray<TTuple<int64, int64>> DelayedEntries;
			DelayedEntries.SetNum(NumEdges);

			if (InEdgeData)
			{
				// We'll cherry pick existing edges
				TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CherryPickInheritedEdges);

				TArray<int32> ReadEdgeIndices;
				TArray<int32> WriteEdgeIndices;

				ReadEdgeIndices.SetNumUninitialized(NumEdges);
				WriteEdgeIndices.SetNumUninitialized(NumEdges);

				const TConstPCGValueRange<int64> InMetadataEntries = InEdgeData->GetConstMetadataEntryValueRange();
				std::atomic<int32> WriteIndex(0);

				PCGEX_PARALLEL_FOR(
					NumEdges,
					const FEdge& OE = ParentGraphEdges[Edges[i].Index];

					// Hijack edge IOIndex to store original edge index in the flattened
					FlattenedEdges[i] = FEdge(i, ParentGraphNodes[OE.Start].PointIndex, ParentGraphNodes[OE.End].PointIndex, i, OE.Index);

					const int32 OriginalPointIndex = OE.PointIndex;
					int64 ParentEntry = PCGInvalidEntryKey;

					if (InMetadataEntries.IsValidIndex(OriginalPointIndex))
					{
					// Grab existing metadata entry & cache read/write indices
					ParentEntry = InMetadataEntries[OriginalPointIndex];
					const int32 LocalWriteIndex = WriteIndex.fetch_add(1, std::memory_order_relaxed);
					ReadEdgeIndices[LocalWriteIndex] = OriginalPointIndex;
					WriteEdgeIndices[LocalWriteIndex] = i;
					}

					OutMetadataEntries[i] = Metadata->AddEntryPlaceholder();
					DelayedEntries[i] = MakeTuple(OutMetadataEntries[i], ParentEntry);
				)

				ReadEdgeIndices.SetNum(WriteIndex);
				WriteEdgeIndices.SetNum(WriteIndex);

				EdgesDataFacade->Source->InheritProperties(ReadEdgeIndices, WriteEdgeIndices, AllocateProperties);
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CreatePoints);

				PCGEX_PARALLEL_FOR(
					NumEdges,
					const FEdge& E = ParentGraphEdges[Edges[i].Index];
					FlattenedEdges[i] = FEdge(i, ParentGraphNodes[E.Start].PointIndex, ParentGraphNodes[E.End].PointIndex, i, E.Index);

					OutMetadataEntries[i] = Metadata->AddEntryPlaceholder();
					DelayedEntries[i] = MakeTuple(OutMetadataEntries[i], PCGInvalidEntryKey);
				)
			}

			Metadata->AddDelayedEntries(DelayedEntries);
		}

		MetadataDetails = InBuilder->GetMetadataDetails();
		const bool bHasUnionMetadata = (MetadataDetails && InBuilder && !ParentGraph->EdgeMetadata.IsEmpty());

#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(IsSubEdge, bool, false, bIsSubEdge) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(bHasUnionMetadata && MetadataDetails->bWrite##_NAME){ _NAME##Buffer = EdgesDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::New); }
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

		if (InBuilder->SourceEdgeFacades && ParentGraph->EdgesUnion)
		{
			UnionBlender = MakeShared<PCGExBlending::FUnionBlender>(MetadataDetails->EdgesBlendingDetailsPtr, MetadataDetails->EdgesCarryOverDetails, PCGExDetails::GetNoneDistances());
			UnionBlender->AddSources(*InBuilder->SourceEdgeFacades, &ProtectedClusterAttributes);
			if (!UnionBlender->Init(TaskManager->GetContext(), EdgesDataFacade, ParentGraph->EdgesUnion))
			{
				// TODO : Log error
				return;
			}
		}

		if (InBuilder->OutputDetails->bOutputEdgeLength)
		{
			if (!PCGExMetaHelpers::IsWritableAttributeName(InBuilder->OutputDetails->EdgeLengthName))
			{
				PCGE_LOG_C(Error, GraphAndLog, TaskManager->GetContext(), FTEXT("Invalid user-defined attribute name for Edge Length."));
			}
			else
			{
				EdgeLength = EdgesDataFacade->GetWritable<double>(InBuilder->OutputDetails->EdgeLengthName, 0, true, PCGExData::EBufferInit::New);
			}
		}

		PCGEX_ASYNC_SUBGROUP_REQ_CHKD_VOID(TaskManager, InParentHandle.Pin(), CompileSubGraph)

		CompileSubGraph->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->CompilationComplete();
		};

		CompileSubGraph->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			This->CompileRange(Scope);
		};

		CompileSubGraph->StartSubLoops(FlattenedEdges.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FSubGraph::CompileRange(const PCGExMT::FScope& Scope)
	{
		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();
		const TSharedPtr<FGraphBuilder> Builder = WeakBuilder.Pin();

		if (!ParentGraph || !Builder) { return; }

		const TSharedPtr<PCGExData::TBuffer<int64>> EdgeEndpointsWriter = EdgesDataFacade->GetWritable<int64>(Attr_PCGExEdgeIdx, -1, false, PCGExData::EBufferInit::New);


		UPCGBasePointData* OutVtxData = VtxDataFacade->GetOut();
		UPCGBasePointData* OutEdgeData = EdgesDataFacade->GetOut();

		TConstPCGValueRange<FTransform> VtxTransforms = OutVtxData->GetConstTransformValueRange();

		TPCGValueRange<int32> EdgeSeeds = OutEdgeData->GetSeedValueRange(false);

		const bool bHasUnionMetadata = (MetadataDetails && !ParentGraph->EdgeMetadata.IsEmpty());
		const FVector SeedOffset = FVector(EdgesDataFacade->Source->IOIndex);

		TArray<PCGExData::FWeightedPoint> WeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;

		if (UnionBlender) { UnionBlender->InitTrackers(Trackers); }

		PCGEX_SCOPE_LOOP(i)
		{
			const FEdge& E = FlattenedEdges[i];
			const int32 EdgeIndex = E.Index; // TODO : This is now i, anyway?

			const int32 Start = E.Start;
			const int32 End = E.End;

			PCGExData::FMutablePoint EdgePt = EdgesDataFacade->GetOutPoint(EdgeIndex);

			if (bHasUnionMetadata)
			{
				const FGraphEdgeMetadata* EdgeMeta = ParentGraph->FindEdgeMetadata_Unsafe(E.IOIndex);
				if (const FGraphEdgeMetadata* RootEdgeMeta = EdgeMeta ? ParentGraph->FindEdgeMetadata_Unsafe(EdgeMeta->RootIndex) : nullptr)
				{
					if (TSharedPtr<PCGExData::IUnionData> UnionData = ParentGraph->EdgesUnion->Get(RootEdgeMeta->RootIndex); UnionBlender && UnionData) { UnionBlender->MergeSingle(EdgeIndex, UnionData, WeightedPoints, Trackers); }

					// TODO : Add Sub-edge edge (is the result of a subdivision + merge)

					if (IsEdgeUnionBuffer) { IsEdgeUnionBuffer->SetValue(EdgeIndex, RootEdgeMeta->IsUnion() || EdgeMeta->IsUnion()); }
					if (IsSubEdgeBuffer) { IsSubEdgeBuffer->SetValue(EdgeIndex, RootEdgeMeta->bIsSubEdge || EdgeMeta->bIsSubEdge); }
					if (EdgeUnionSizeBuffer)
					{
						EdgeUnionSizeBuffer->SetValue(EdgeIndex, EdgeMeta != RootEdgeMeta ? RootEdgeMeta->UnionSize + EdgeMeta->UnionSize : RootEdgeMeta->UnionSize);
					}
				}
			}

			EdgeEndpointsWriter->SetValue(EdgeIndex, PCGEx::H64(Start, End));

			if (Builder->OutputDetails->bWriteEdgePosition)
			{
				Builder->OutputDetails->BasicEdgeSolidification.Mutate(EdgePt, VtxDataFacade->GetOutPoint(Start), VtxDataFacade->GetOutPoint(End), Builder->OutputDetails->EdgePosition);
			}

			if (EdgeLength) { EdgeLength->SetValue(EdgeIndex, FVector::Dist(VtxTransforms[Start].GetLocation(), VtxTransforms[End].GetLocation())); }

			if (EdgeSeeds[EdgeIndex] == 0 || ParentGraph->bRefreshEdgeSeed) { EdgeSeeds[EdgeIndex] = PCGExRandom::ComputeSpatialSeed(EdgePt.GetLocation(), SeedOffset); }
		}
	}

	void FSubGraph::CompilationComplete()
	{
		UnionBlender.Reset();

		const TSharedPtr<PCGExMT::FTaskManager>& TaskManager = WeakTaskManager.Pin();
		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();
		if (!TaskManager || !TaskManager->IsAvailable() || !ParentGraph) { return; }

		PCGEX_SHARED_THIS_DECL

		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters && ParentGraph->bBuildClusters)
		{
			if (Cast<UPCGExClusterEdgesData>(EdgesDataFacade->Source->GetOut()))
			{
				PCGEX_LAUNCH(PCGExGraphTask::FWriteSubGraphCluster, ThisPtr)
			}
		}

		if (OnSubGraphPostProcess) { OnSubGraphPostProcess(ThisPtr.ToSharedRef()); }
		EdgesDataFacade->WriteFastest(TaskManager);
	}

#undef PCGEX_FOREACH_EDGE_METADATA
}

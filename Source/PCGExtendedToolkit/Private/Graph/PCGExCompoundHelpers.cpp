// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCompoundHelpers.h"

namespace PCGExGraph
{
	// Need :
	// - GraphBuilder
	// - CompoundGraph
	// - CompoundPoints

	// - PointEdgeIntersectionSettings


	FCompoundProcessor::FCompoundProcessor(
		FPCGExPointsProcessorContext* InContext,
		FPCGExPointPointIntersectionDetails InPointPointIntersectionSettings,
		FPCGExBlendingDetails InDefaultPointsBlending,
		FPCGExBlendingDetails InDefaultEdgesBlending):
		Context(InContext),
		PointPointIntersectionDetails(InPointPointIntersectionSettings),
		DefaultPointsBlendingDetails(InDefaultPointsBlending),
		DefaultEdgesBlendingDetails(InDefaultEdgesBlending)
	{
	}

	FCompoundProcessor::~FCompoundProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(PointEdgeIntersections)
		PCGEX_DELETE(EdgeEdgeIntersections)
		PCGEX_DELETE(MetadataBlender)
	}

	void FCompoundProcessor::InitPointEdge(
		const FPCGExPointEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoPointEdge = true;
		PointEdgeIntersectionDetails = InDetails;
		bUseCustomPointEdgeBlending = bUseCustom;
		if (InOverride) { CustomPointEdgeBlendingDetails = *InOverride; }
	}

	void FCompoundProcessor::InitEdgeEdge(
		const FPCGExEdgeEdgeIntersectionDetails& InDetails,
		const bool bUseCustom,
		const FPCGExBlendingDetails* InOverride)
	{
		bDoEdgeEdge = true;
		EdgeEdgeIntersectionDetails = InDetails;
		bUseCustomEdgeEdgeBlending = bUseCustom;
		if (InOverride) { CustomEdgeEdgeBlendingDetails = *InOverride; }
	}

	bool FCompoundProcessor::Execute()
	{
		if (!bRunning) { return false; }

		if (Context->IsState(State_FindingPointEdgeIntersections))
		{
			auto PointEdge = [&](const int32 EdgeIndex)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[EdgeIndex];
				if (!Edge.bValid) { return; }
				FindCollinearNodes(PointEdgeIntersections, EdgeIndex, CompoundFacade->Source->GetOut());
			};

			if (!Context->Process(PointEdge, GraphBuilder->Graph->Edges.Num())) { return false; }

			PointEdgeIntersections->Insert(); // TODO : Async?
			CompoundFacade->Source->CleanupKeys();

			Context->SetState(State_BlendingPointEdgeCrossings);
		}

		if (Context->IsState(State_BlendingPointEdgeCrossings))
		{
			auto Initialize = [&]()
			{
				if (bUseCustomPointEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomPointEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out, true);
			};

			auto BlendPointEdgeMetadata = [&](const int32 Index)
			{
				// TODO
			};

			if (!Context->Process(Initialize, BlendPointEdgeMetadata, PointEdgeIntersections->Edges.Num())) { return false; }

			if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }

			PCGEX_DELETE(PointEdgeIntersections)
			Context->SetAsyncState(PCGExMT::State_CompoundWriting);
		}

		if (Context->IsState(PCGExMT::State_CompoundWriting))
		{
			if (MetadataBlender) { PCGEX_ASYNC_WAIT }
			PCGEX_DELETE(MetadataBlender)

			if (bDoEdgeEdge) { FindEdgeEdgeIntersections(); }
			else { Context->SetState(State_WritingClusters); }
		}

		if (Context->IsState(State_FindingEdgeEdgeIntersections))
		{
			auto EdgeEdge = [&](const int32 EdgeIndex)
			{
				const FIndexedEdge& Edge = GraphBuilder->Graph->Edges[EdgeIndex];
				if (!Edge.bValid) { return; }
				FindOverlappingEdges(EdgeEdgeIntersections, EdgeIndex);
			};

			if (!Context->Process(EdgeEdge, GraphBuilder->Graph->Edges.Num())) { return false; }

			EdgeEdgeIntersections->Insert(); // TODO : Async?
			CompoundFacade->Source->CleanupKeys();

			Context->SetState(State_BlendingEdgeEdgeCrossings);
		}

		if (Context->IsState(State_BlendingEdgeEdgeCrossings))
		{
			auto Initialize = [&]()
			{
				if (bUseCustomPointEdgeBlending) { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&CustomEdgeEdgeBlendingDetails); }
				else { MetadataBlender = new PCGExDataBlending::FMetadataBlender(&DefaultPointsBlendingDetails); }

				MetadataBlender->PrepareForData(CompoundFacade, PCGExData::ESource::Out, true);
			};

			auto BlendCrossingMetadata = [&](const int32 Index) { EdgeEdgeIntersections->BlendIntersection(Index, MetadataBlender); };

			if (!Context->Process(Initialize, BlendCrossingMetadata, EdgeEdgeIntersections->Crossings.Num())) { return false; }

			if (MetadataBlender) { CompoundFacade->Write(Context->GetAsyncManager(), true); }

			PCGEX_DELETE(EdgeEdgeIntersections)
			Context->SetAsyncState(PCGExMT::State_MetaWriting2);
		}

		if (Context->IsState(PCGExMT::State_MetaWriting2))
		{
			if (MetadataBlender) { PCGEX_ASYNC_WAIT }
			PCGEX_DELETE(MetadataBlender)

			Context->SetState(State_WritingClusters);
		}

		if (Context->IsState(State_WritingClusters))
		{
			GraphBuilder->CompileAsync(Context->GetAsyncManager(), &GraphMetadataDetails);
			Context->SetAsyncState(State_Compiling);
			return false;
		}

		if (Context->IsState(State_Compiling))
		{
			PCGEX_ASYNC_WAIT

			if (!GraphBuilder->bCompiledSuccessfully)
			{
				CompoundFacade->Source->InitializeOutput(PCGExData::EInit::NoOutput);
				return true;
			}

			GraphBuilder->Write(Context);
			return true;
		}

		if (Context->IsState(State_MergingEdgeCompounds))
		{
			//TODO	
		}

		return true;
	}

	bool FCompoundProcessor::IsRunning() const { return bRunning; }

	void FCompoundProcessor::FindPointEdgeIntersections()
	{
		PointEdgeIntersections = new FPointEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &PointEdgeIntersectionDetails);

		Context->SetState(State_FindingPointEdgeIntersections);
	}

	void FCompoundProcessor::FindEdgeEdgeIntersections()
	{
		EdgeEdgeIntersections = new FEdgeEdgeIntersections(
			GraphBuilder->Graph, CompoundGraph, CompoundFacade->Source, &EdgeEdgeIntersectionDetails);

		Context->SetState(State_FindingEdgeEdgeIntersections);
	}
}

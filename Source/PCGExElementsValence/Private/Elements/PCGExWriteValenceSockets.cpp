// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExWriteValenceSockets.h"

#include "Data/PCGExData.h"
#include "Clusters/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExWriteValenceSockets"
#define PCGEX_NAMESPACE WriteValenceSockets

PCGExData::EIOInit UPCGExWriteValenceSocketsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExWriteValenceSocketsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExWriteValenceSocketsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

void FPCGExWriteValenceSocketsContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExWriteValenceSocketsSettings* Settings = GetInputSettings<UPCGExWriteValenceSocketsSettings>();
	if (Settings && !Settings->SocketCollection.IsNull())
	{
		AddAssetDependency(Settings->SocketCollection.ToSoftObjectPath());
	}
}

PCGEX_INITIALIZE_ELEMENT(WriteValenceSockets)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(WriteValenceSockets)

bool FPCGExWriteValenceSocketsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteValenceSockets)

	// Load socket collection
	if (!Context->SocketCollection)
	{
		if (!Settings->SocketCollection.IsNull())
		{
			Context->SocketCollection = Settings->SocketCollection.LoadSynchronous();
		}
	}

	if (!Context->SocketCollection)
	{
		if (!Settings->bQuietMissingCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Valence Socket Collection provided."));
		}
		return false;
	}

	// Validate collection
	TArray<FText> ValidationErrors;
	if (!Context->SocketCollection->Validate(ValidationErrors))
	{
		for (const FText& Error : ValidationErrors)
		{
			PCGE_LOG(Error, GraphAndLog, Error);
		}
		return false;
	}

	// Build socket cache for fast processing
	if (!Context->SocketCache.BuildFrom(Context->SocketCollection))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build socket cache from collection."));
		return false;
	}

	return true;
}

void FPCGExWriteValenceSocketsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(WriteValenceSockets)

	if (!Context->SocketCollection && !Settings->SocketCollection.IsNull())
	{
		Context->SocketCollection = Settings->SocketCollection.Get();
	}
}

bool FPCGExWriteValenceSocketsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteValenceSockets)

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteValenceSockets
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteValenceSockets::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const FName IdxAttributeName = Context->SocketCollection->GetIdxAttributeName();

		// Initialize all edge indices to sentinel values (no match)
		constexpr int64 InitialValue = static_cast<int64>(PCGExValence::NO_SOCKET_MATCH) | (static_cast<int64>(PCGExValence::NO_SOCKET_MATCH) << 8);
		IdxWriter = EdgeDataFacade->GetWritable<int64>(IdxAttributeName, InitialValue, false, PCGExData::EBufferInit::New);
		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
		TConstPCGValueRange<FTransform> InTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();

		TSharedPtr<PCGExData::TArrayBuffer<int64>> IdxArrayWriter = StaticCastSharedPtr<PCGExData::TArrayBuffer<int64>>(IdxWriter);
		TArray<int64>* EdgeIndices = IdxArrayWriter->GetOutValues().Get();
		TArray<PCGExGraphs::FEdge>& Edges = *Cluster->Edges.Get();

		// Use cached socket data for fast lookup
		const PCGExValence::FSocketCache& Cache = Context->SocketCache;
		const bool bUseTransform = Cache.bTransformDirection;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExClusters::FNode& Node = Nodes[Index];

			const FTransform& DirTransform = bUseTransform ? InTransforms[Node.PointIndex] : FTransform::Identity;

			int64 SocketMask = 0;

			// Process each link from this node
			for (const PCGExClusters::FLink& Link : Node.Links)
			{
				const int32 EdgeIndex = Link.Edge;
				const int32 NeighborNodeIndex = Link.Node;

				// Get direction to neighbor
				// TODO : Optimize this we can use InTransform directly
				const FVector Direction = Cluster->GetDir(Node.Index, NeighborNodeIndex);

				// Find matching socket using cached data
				const uint8 SocketIndex = Cache.FindMatchingSocket(Direction, bUseTransform, DirTransform);

				// Write socket index directly to the appropriate byte offset
				// Start node writes to byte 0, end node writes to byte 1
				// This avoids race conditions since each node writes to a different byte
				const PCGExGraphs::FEdge& Edge = Edges[EdgeIndex];
				uint8* PackedBytes = reinterpret_cast<uint8*>(EdgeIndices->GetData() + EdgeIndex);
				const int32 ByteOffset = (Edge.Start == Node.PointIndex) ? 0 : 1;

				if (SocketIndex != PCGExValence::NO_SOCKET_MATCH)
				{
					// Get the bitmask directly from cache
					SocketMask |= Cache.GetBitmask(SocketIndex);
					PackedBytes[ByteOffset] = SocketIndex;
				}
				else
				{
					PackedBytes[ByteOffset] = PCGExValence::NO_SOCKET_MATCH;
					FPlatformAtomics::InterlockedIncrement(&NoMatchCount);
				}
			}

			// Write vertex socket mask
			if (VertexMasks)
			{
				(*VertexMasks)[Node.PointIndex] = SocketMask;
			}
		}
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		EdgeDataFacade->WriteFastest(TaskManager);

		if (NoMatchCount > 0 && Settings->bWarnOnNoMatch)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				           FTEXT("Valence Sockets: {0} edge directions did not match any socket."),
				           FText::AsNumber(NoMatchCount)));
		}
	}

	//////// BATCH

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteValenceSockets)

		if (!Context->SocketCollection) { return; }

		const FName MaskAttributeName = Context->SocketCollection->GetMaskAttributeName();

		// Create vertex mask writer
		const TSharedPtr<PCGExData::TBuffer<int64>> MaskWriter = VtxDataFacade->GetWritable<int64>(MaskAttributeName, 0, false, PCGExData::EBufferInit::Inherit);
		const TSharedPtr<PCGExData::TArrayBuffer<int64>> MaskArrayWriter = StaticCastSharedPtr<PCGExData::TArrayBuffer<int64>>(MaskWriter);
		VertexMasks = MaskArrayWriter->GetOutValues();

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }

		FProcessor* TypedProcessor = static_cast<FProcessor*>(InProcessor.Get());
		TypedProcessor->VertexMasks = VertexMasks;

		return true;
	}

	void FBatch::CompleteWork()
	{
		VtxDataFacade->WriteFastest(TaskManager);
		TBatch<FProcessor>::CompleteWork();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

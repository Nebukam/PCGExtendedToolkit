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
		TArray<int64>& EdgeIndices = *IdxArrayWriter->GetOutValues().Get();
		TArray<PCGExGraphs::FEdge>& Edges = *Cluster->Edges.Get();

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExClusters::FNode& Node = Nodes[Index];
			if (!Context->SocketCollection) { return; }

			const UPCGExValenceSocketCollection* Collection = Context->SocketCollection;
			const bool bUseTransform = Collection->bTransformDirection;

			// Get point transform if needed
			const FTransform& PointTransform = bUseTransform ? InTransforms[Node.PointIndex] : FTransform::Identity;

			int64 SocketMask = 0;

			// Process each link from this node
			for (const PCGExClusters::FLink& Link : Node.Links)
			{
				const int32 EdgeIndex = Link.Edge;
				const int32 NeighborNodeIndex = Link.Node;

				// Get direction to neighbor
				const FVector Direction = Cluster->GetDir(Node.Index, NeighborNodeIndex);

				// Find matching socket
				const uint8 SocketIndex = Collection->FindMatchingSocket(Direction, bUseTransform, PointTransform);

				if (SocketIndex != PCGExValence::NO_SOCKET_MATCH)
				{
					// Get the bitmask for this socket
					FVector SocketDir;
					int64 SocketBitmask;
					if (Collection->Sockets[SocketIndex].GetDirectionAndBitmask(SocketDir, SocketBitmask))
					{
						SocketMask |= SocketBitmask;
					}

					// Update edge indices
					// Determine if this node is start or end of the edge
					{
						const PCGExGraphs::FEdge& Edge = Edges[EdgeIndex];
						int64& PackedIndices = EdgeIndices[EdgeIndex];

						if (Edge.Start == Node.PointIndex)
						{
							// This node is the start - store in lower byte
							PackedIndices = (PackedIndices & 0xFFFFFFFFFFFFFF00LL) | static_cast<int64>(SocketIndex);
						}
						else
						{
							// This node is the end - store in second byte
							PackedIndices = (PackedIndices & 0xFFFFFFFFFFFF00FFLL) | (static_cast<int64>(SocketIndex) << 8);
						}
					}
				}
				else
				{
					// No match - store sentinel value
					{
						const PCGExGraphs::FEdge& Edge = Edges[EdgeIndex];
						int64& PackedIndices = EdgeIndices[EdgeIndex];

						if (Edge.Start == Node.PointIndex)
						{
							PackedIndices = (PackedIndices & 0xFFFFFFFFFFFFFF00LL) | static_cast<int64>(PCGExValence::NO_SOCKET_MATCH);
						}
						else
						{
							PackedIndices = (PackedIndices & 0xFFFFFFFFFFFF00FFLL) | (static_cast<int64>(PCGExValence::NO_SOCKET_MATCH) << 8);
						}
					}

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

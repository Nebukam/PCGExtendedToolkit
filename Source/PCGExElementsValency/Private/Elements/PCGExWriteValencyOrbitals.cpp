// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExWriteValencyOrbitals.h"

#include "Data/PCGExData.h"
#include "Clusters/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExWriteValencyOrbitals"
#define PCGEX_NAMESPACE WriteValencyOrbitals

PCGExData::EIOInit UPCGExWriteValencyOrbitalsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExWriteValencyOrbitalsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExWriteValencyOrbitalsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

void FPCGExWriteValencyOrbitalsContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExWriteValencyOrbitalsSettings* Settings = GetInputSettings<UPCGExWriteValencyOrbitalsSettings>();
	if (Settings && !Settings->OrbitalSet.IsNull())
	{
		AddAssetDependency(Settings->OrbitalSet.ToSoftObjectPath());
	}
}

PCGEX_INITIALIZE_ELEMENT(WriteValencyOrbitals)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(WriteValencyOrbitals)

bool FPCGExWriteValencyOrbitalsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteValencyOrbitals)

	// TODO : Move this to PostBoot
	
	// Load orbital set
	if (!Context->OrbitalSet)
	{
		if (!Settings->OrbitalSet.IsNull())
		{
			// TODO : LoadBLocking, we're not in game thread!
			Context->OrbitalSet = Settings->OrbitalSet.LoadSynchronous();
		}
	}

	if (!Context->OrbitalSet)
	{
		if (!Settings->bQuietMissingOrbitalSet)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Valency Orbital Set provided."));
		}
		return false;
	}

	// Validate orbital set
	TArray<FText> ValidationErrors;
	if (!Context->OrbitalSet->Validate(ValidationErrors))
	{
		for (const FText& Error : ValidationErrors)
		{
			PCGE_LOG(Error, GraphAndLog, Error);
		}
		return false;
	}

	// Build orbital cache for fast processing
	if (!Context->OrbitalCache.BuildFrom(Context->OrbitalSet))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build orbital cache from orbital set."));
		return false;
	}

	return true;
}

void FPCGExWriteValencyOrbitalsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(WriteValencyOrbitals)

	if (!Context->OrbitalSet && !Settings->OrbitalSet.IsNull())
	{
		Context->OrbitalSet = Settings->OrbitalSet.Get();
	}
}

bool FPCGExWriteValencyOrbitalsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteValencyOrbitals)

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

namespace PCGExWriteValencyOrbitals
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteValencyOrbitals::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const FName IdxAttributeName = Context->OrbitalSet->GetOrbitalIdxAttributeName();

		// Initialize all edge indices to sentinel values (no match)
		constexpr int64 InitialValue = static_cast<int64>(PCGExValency::NO_ORBITAL_MATCH) | (static_cast<int64>(PCGExValency::NO_ORBITAL_MATCH) << 8);
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

		// Use cached orbital data for fast lookup
		const PCGExValency::FOrbitalCache& Cache = Context->OrbitalCache;
		const bool bUseTransform = Cache.bTransformOrbital;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExClusters::FNode& Node = Nodes[Index];

			const FTransform& DirTransform = bUseTransform ? InTransforms[Node.PointIndex] : FTransform::Identity;

			int64 OrbitalMask = 0;

			// Process each link from this node
			for (const PCGExClusters::FLink& Link : Node.Links)
			{
				const int32 EdgeIndex = Link.Edge;
				const int32 NeighborNodeIndex = Link.Node;

				// Get direction to neighbor
				// TODO : Optimize this we can use InTransform directly
				const FVector Direction = Cluster->GetDir(Node.Index, NeighborNodeIndex);

				// Find matching orbital using cached data
				const uint8 OrbitalIndex = Cache.FindMatchingOrbital(Direction, bUseTransform, DirTransform);

				// Write orbital index directly to the appropriate byte offset
				// Start node writes to byte 0, end node writes to byte 1
				// This avoids race conditions since each node writes to a different byte
				const PCGExGraphs::FEdge& Edge = Edges[EdgeIndex];
				uint8* PackedBytes = reinterpret_cast<uint8*>(EdgeIndices->GetData() + EdgeIndex);
				const int32 ByteOffset = (Edge.Start == Node.PointIndex) ? 0 : 1;

				if (OrbitalIndex != PCGExValency::NO_ORBITAL_MATCH)
				{
					// Get the bitmask directly from cache
					OrbitalMask |= Cache.GetBitmask(OrbitalIndex);
					PackedBytes[ByteOffset] = OrbitalIndex;
				}
				else
				{
					PackedBytes[ByteOffset] = PCGExValency::NO_ORBITAL_MATCH;
					FPlatformAtomics::InterlockedIncrement(&NoMatchCount);
				}
			}

			// Write vertex orbital mask
			if (VertexMasks)
			{
				(*VertexMasks)[Node.PointIndex] = OrbitalMask;
			}
		}
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		EdgeDataFacade->WriteFastest(TaskManager);

		if (NoMatchCount > 0 && Settings->bWarnOnNoMatch)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				           FTEXT("Valency Orbitals: {0} edge directions did not match any orbital."),
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteValencyOrbitals)

		if (!Context->OrbitalSet) { return; }

		const FName MaskAttributeName = Context->OrbitalSet->GetOrbitalMaskAttributeName();

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

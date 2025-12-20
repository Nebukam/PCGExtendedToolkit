// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cluster/PCGExClusterHelpers.h"

#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Cluster/PCGExCluster.h"
#include "Cluster/PCGExGraphLabels.h"
#include "Paths/PCGExPathLabels.h"

namespace PCGExCluster
{
	namespace Helpers
	{
		void GetAdjacencyData(const FCluster* InCluster, FNode& InNode, TArray<FAdjacencyData>& OutData)
		{
			const int32 NumAdjacency = InNode.Num();
			const FVector NodePosition = InCluster->GetPos(InNode);
			OutData.Reserve(NumAdjacency);
			for (int i = 0; i < NumAdjacency; i++)
			{
				const FLink Lk = InNode.Links[i];

				const FNode* OtherNode = InCluster->NodesDataPtr + Lk.Node;
				const FVector OtherPosition = InCluster->GetPos(OtherNode);

				FAdjacencyData& Data = OutData.Emplace_GetRef();
				Data.NodeIndex = Lk.Node;
				Data.NodePointIndex = OtherNode->PointIndex;
				Data.EdgeIndex = Lk.Edge;
				Data.Direction = (NodePosition - OtherPosition).GetSafeNormal();
				Data.Length = FVector::Dist(NodePosition, OtherPosition);
			}
		}

		void SetClusterVtx(const TSharedPtr<PCGExData::FPointIO>& IO, PCGExDataId& OutId)
		{
			OutId = IO->Tags->Set<int64>(PCGExGraph::Labels::TagStr_PCGExCluster, IO->GetOutIn()->GetUniqueID());
			IO->Tags->AddRaw(PCGExGraph::Labels::TagStr_PCGExVtx);
			IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExEdges);
		}

		void MarkClusterVtx(const TSharedPtr<PCGExData::FPointIO>& IO, const PCGExDataId& Id)
		{
			IO->Tags->Set(PCGExGraph::Labels::TagStr_PCGExCluster, Id);
			IO->Tags->AddRaw(PCGExGraph::Labels::TagStr_PCGExVtx);
			IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExEdges);
			IO->DeleteAttribute(PCGExPath::Labels::ClosedLoopIdentifier);
		}

		void MarkClusterEdges(const TSharedPtr<PCGExData::FPointIO>& IO, const PCGExDataId& Id)
		{
			IO->Tags->Set(PCGExGraph::Labels::TagStr_PCGExCluster, Id);
			IO->Tags->AddRaw(PCGExGraph::Labels::TagStr_PCGExEdges);
			IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExVtx);
			IO->DeleteAttribute(PCGExPath::Labels::ClosedLoopIdentifier);
		}

		void MarkClusterEdges(const TArrayView<TSharedRef<PCGExData::FPointIO>> Edges, const PCGExDataId& Id)
		{
			for (const TSharedRef<PCGExData::FPointIO>& IO : Edges) { MarkClusterEdges(IO, Id); }
		}

		bool IsPointDataVtxReady(const UPCGMetadata* Metadata)
		{
			return PCGExMetaHelpers::TryGetConstAttribute<int64>(Metadata, PCGExGraph::Labels::Attr_PCGExVtxIdx) ? true : false;
		}

		bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
		{
			return PCGExMetaHelpers::TryGetConstAttribute<int64>(Metadata, PCGExGraph::Labels::Attr_PCGExEdgeIdx) ? true : false;
		}

		void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
		{
			if (!PointIO->GetOut()) { return; }
			UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
			PointIO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExCluster);
			PointIO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExVtx);
			Metadata->DeleteAttribute(PCGExGraph::Labels::Attr_PCGExVtxIdx);
			Metadata->DeleteAttribute(PCGExGraph::Labels::Attr_PCGExEdgeIdx);
		}

		void CleanupEdgeData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
		{
			if (!PointIO->GetOut()) { return; }
			UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
			PointIO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExCluster);
			PointIO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExEdges);
			Metadata->DeleteAttribute(PCGExGraph::Labels::Attr_PCGExVtxIdx);
			Metadata->DeleteAttribute(PCGExGraph::Labels::Attr_PCGExEdgeIdx);
		}

		void CleanupClusterData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
		{
			CleanupVtxData(PointIO);
			CleanupEdgeData(PointIO);
			CleanupClusterTags(PointIO);
		}

		void CleanupClusterTags(const TSharedPtr<PCGExData::FPointIO>& IO, const bool bKeepPairTag)
		{
			IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExVtx);
			IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExEdges);
			if (!bKeepPairTag) { IO->Tags->Remove(PCGExGraph::Labels::TagStr_PCGExCluster); }
		}
	}
}

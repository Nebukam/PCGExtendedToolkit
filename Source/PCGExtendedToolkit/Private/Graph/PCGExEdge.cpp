// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdge.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExDataValue.h"
#include "Paths/PCGExPaths.h"

namespace PCGExGraph
{
	void SetClusterVtx(const TSharedPtr<PCGExData::FPointIO>& IO, PCGExCommon::DataIDType& OutId)
	{
		OutId = IO->Tags->Set<int32>(TagStr_PCGExCluster, IO->GetOutIn()->GetUniqueID());
		IO->Tags->AddRaw(TagStr_PCGExVtx);
		IO->Tags->Remove(TagStr_PCGExEdges);
	}

	void MarkClusterVtx(const TSharedPtr<PCGExData::FPointIO>& IO, const PCGExCommon::DataIDType& Id)
	{
		IO->Tags->Set(TagStr_PCGExCluster, Id);
		IO->Tags->AddRaw(TagStr_PCGExVtx);
		IO->Tags->Remove(TagStr_PCGExEdges);
		IO->DeleteAttribute(PCGExPaths::ClosedLoopIdentifier);
	}

	void MarkClusterEdges(const TSharedPtr<PCGExData::FPointIO>& IO, const PCGExCommon::DataIDType& Id)
	{
		IO->Tags->Set(TagStr_PCGExCluster, Id);
		IO->Tags->AddRaw(TagStr_PCGExEdges);
		IO->Tags->Remove(TagStr_PCGExVtx);
		IO->DeleteAttribute(PCGExPaths::ClosedLoopIdentifier);
	}

	void MarkClusterEdges(const TArrayView<TSharedRef<PCGExData::FPointIO>> Edges, const PCGExCommon::DataIDType& Id)
	{
		for (const TSharedRef<PCGExData::FPointIO>& IO : Edges) { MarkClusterEdges(IO, Id); }
	}

	void CleanupClusterTags(const TSharedPtr<PCGExData::FPointIO>& IO, const bool bKeepPairTag)
	{
		IO->Tags->Remove(TagStr_PCGExVtx);
		IO->Tags->Remove(TagStr_PCGExEdges);
		if (!bKeepPairTag) { IO->Tags->Remove(TagStr_PCGExCluster); }
	}
}

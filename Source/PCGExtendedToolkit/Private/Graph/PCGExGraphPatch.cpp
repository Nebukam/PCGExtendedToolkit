// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraphPatch.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExGraphParamsData.h"

#include "Graph/PCGExGraph.h"

void UPCGExGraphPatch::Add(int32 InIndex)
{
	{
		FReadScopeLock ReadLock(HashLock);
		if (IndicesSet.Contains(InIndex)) { return; }
	}

	{
		FWriteScopeLock WriteLock(HashLock);
		IndicesSet.Add(InIndex);
	}
	{
		FWriteScopeLock WriteLock(Parent->HashLock);
		Parent->IndicesMap.Add(InIndex, this);
	}
}

bool UPCGExGraphPatch::Contains(const int32 InIndex) const
{
	FReadScopeLock ReadLock(HashLock);
	return IndicesSet.Contains(InIndex);
}

void UPCGExGraphPatch::AddEdge(uint64 InEdgeHash)
{
	PCGExGraph::FEdge Edge = static_cast<PCGExGraph::FEdge>(InEdgeHash);
	Add(Edge.Start);
	Add(Edge.End);
	{
		FWriteScopeLock WriteLock(HashLock);
		EdgesHashSet.Add(InEdgeHash);
	}
}

bool UPCGExGraphPatch::ContainsEdge(const uint64 InEdgeHash) const
{
	FReadScopeLock ReadLock(HashLock);
	return EdgesHashSet.Contains(InEdgeHash);
}

bool UPCGExGraphPatch::OutputTo(const UPCGExPointIO* OutIO, int32 PatchIDOverride)
{
	const TArray<FPCGPoint>& InPoints = OutIO->In->GetPoints();
	TArray<FPCGPoint>& Points = OutIO->Out->GetMutablePoints();
	Points.Reserve(Points.Num() + IndicesSet.Num());
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchIDAttributeName, PatchIDOverride < 0 ? PatchID : PatchIDOverride);
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchSizeAttributeName, IndicesSet.Num());
	for (const int32 Index : IndicesSet)
	{
		FPCGPoint& NewPoint = Points.Emplace_GetRef(InPoints[Index]);
	}
	return true;
}

void UPCGExGraphPatch::Flush()
{
	IndicesSet.Empty();
	PointIO = nullptr;
	Parent = nullptr;
}

bool UPCGExGraphPatchGroup::Contains(const uint64 Hash) const
{
	FReadScopeLock ReadLock(HashLock);
	return IndicesMap.Contains(Hash);
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::FindPatch(uint64 Hash)
{
	FReadScopeLock ReadLock(HashLock);

	UPCGExGraphPatch** PatchPtr = IndicesMap.Find(Hash);
	if (!PatchPtr) { return nullptr; }

	return *PatchPtr;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::GetOrCreatePatch(const uint64 Hash)
{
	{
		FReadScopeLock ReadLock(HashLock);
		if (UPCGExGraphPatch** PatchPtr = IndicesMap.Find(Hash)) { return *PatchPtr; }
	}

	UPCGExGraphPatch* NewPatch = CreatePatch();
	NewPatch->Add(Hash);
	return NewPatch;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::CreatePatch()
{
	FWriteScopeLock WriteLock(PatchesLock);
	UPCGExGraphPatch* NewPatch = NewObject<UPCGExGraphPatch>();
	Patches.Add(NewPatch);
	NewPatch->Parent = this;
	NewPatch->PointIO = PointIO;
	NewPatch->PatchID = Patches.Num() - 1;
	return NewPatch;
}

void UPCGExGraphPatchGroup::Distribute(const int32 InIndex, UPCGExGraphPatch* Patch)
{
	if (Patch)
	{
		if (Patch->Contains(InIndex))
		{
			// Exit early since this point index has already been registered in this patch.
			return;
		}

		// Otherwise add index to active patch
		Patch->Add(InIndex);
	}

	TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
	UnsignedEdges.Reserve(NumMaxEdges);

	CurrentGraph->GetEdges(InIndex, PointIO->GetInPoint(InIndex).MetadataEntry, UnsignedEdges, CrawlEdgeTypes);

	for (const PCGExGraph::FUnsignedEdge& UEdge : UnsignedEdges)
	{
		if (!Patch) { Patch = GetOrCreatePatch(InIndex); }
		Distribute(UEdge.End, Patch);
		Patch->AddEdge(UEdge.GetUnsignedHash());
	}
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExPointIO::EInit::NewOutput);
		Patch->OutputTo(OutIO, -1);
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

void UPCGExGraphPatchGroup::Flush()
{
	for (UPCGExGraphPatch* Patch : Patches)
	{
		Patch->Flush();
		Patch->ConditionalBeginDestroy();
	}
	
	Patches.Empty();
	IndicesMap.Empty();
	PointIO = nullptr;
	CurrentGraph = nullptr;
	PatchesIO = nullptr;
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount, const uint32 PUID)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	int32 PatchIndex = 0;
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const int64 OutNumPoints = Patch->IndicesSet.Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { continue; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { continue; }
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExPointIO::EInit::NewOutput);
		OutIO->Out->Metadata->CreateAttribute<int32>(PCGExGraph::PUIDAttributeName, PUID, false, true);
		Patch->OutputTo(OutIO, PatchIndex);
		PatchIndex++;
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

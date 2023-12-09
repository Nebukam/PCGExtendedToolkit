// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraphPatch.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "..\..\Public\Data\PCGExPointIO.h"
#include "Data/PCGExGraphParamsData.h"

#include "Graph/PCGExGraph.h"

FPCGExGraphPatch::~FPCGExGraphPatch()
{
	IndicesSet.Empty();
	EdgesHashSet.Empty();
	PointIO = nullptr;
	Parent = nullptr;
}

void FPCGExGraphPatch::Add(const int32 InIndex)
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

bool FPCGExGraphPatch::Contains(const int32 InIndex) const
{
	FReadScopeLock ReadLock(HashLock);
	return IndicesSet.Contains(InIndex);
}

void FPCGExGraphPatch::AddEdge(uint64 InEdgeHash)
{
	const PCGExGraph::FEdge Edge = static_cast<PCGExGraph::FEdge>(InEdgeHash);
	Add(Edge.Start);
	Add(Edge.End);
	{
		FWriteScopeLock WriteLock(HashLock);
		EdgesHashSet.Add(InEdgeHash);
	}
}

bool FPCGExGraphPatch::ContainsEdge(const uint64 InEdgeHash) const
{
	FReadScopeLock ReadLock(HashLock);
	return EdgesHashSet.Contains(InEdgeHash);
}

bool FPCGExGraphPatch::OutputTo(const FPCGExPointIO& OutIO, int32 PatchIDOverride)
{
	const TArray<FPCGPoint>& InPoints = OutIO.GetIn()->GetPoints();
	UPCGPointData* OutData = OutIO.GetOut();
	TArray<FPCGPoint>& Points = OutData->GetMutablePoints();

	Points.Reserve(Points.Num() + IndicesSet.Num());
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutData->Metadata, Parent->PatchIDAttributeName, PatchIDOverride < 0 ? PatchID : PatchIDOverride);
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutData->Metadata, Parent->PatchSizeAttributeName, IndicesSet.Num());
	for (const int32 Index : IndicesSet)
	{
		FPCGPoint& NewPoint = Points.Emplace_GetRef(InPoints[Index]);
	}
	return true;
}

bool FPCGExGraphPatchGroup::Contains(const uint64 Hash) const
{
	FReadScopeLock ReadLock(HashLock);
	return IndicesMap.Contains(Hash);
}

FPCGExGraphPatch* FPCGExGraphPatchGroup::FindPatch(uint64 Hash)
{
	FReadScopeLock ReadLock(HashLock);

	FPCGExGraphPatch** PatchPtr = IndicesMap.Find(Hash);
	if (!PatchPtr) { return nullptr; }

	return *PatchPtr;
}

FPCGExGraphPatch* FPCGExGraphPatchGroup::GetOrCreatePatch(const uint64 Hash)
{
	{
		FReadScopeLock ReadLock(HashLock);
		if (FPCGExGraphPatch** PatchPtr = IndicesMap.Find(Hash)) { return *PatchPtr; }
	}

	FPCGExGraphPatch* NewPatch = CreatePatch();
	NewPatch->Add(Hash);
	return NewPatch;
}

FPCGExGraphPatch* FPCGExGraphPatchGroup::CreatePatch()
{
	FWriteScopeLock WriteLock(PatchesLock);
	FPCGExGraphPatch* NewPatch = new FPCGExGraphPatch();
	Patches.Add(NewPatch);
	NewPatch->Parent = this;
	NewPatch->PointIO = PointIO;
	NewPatch->PatchID = Patches.Num() - 1;
	return NewPatch;
}

void FPCGExGraphPatchGroup::Distribute(const int32 InIndex, FPCGExGraphPatch* Patch)
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

void FPCGExGraphPatchGroup::OutputTo(FPCGContext* Context)
{
	PatchesIO = new FPCGExPointIOGroup();
	for (FPCGExGraphPatch* Patch : Patches)
	{
		FPCGExPointIO& OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExPointIO::EInit::NewOutput);
		Patch->OutputTo(OutIO, -1);
	}
	PatchesIO->OutputTo(Context);
	delete PatchesIO;
}

FPCGExGraphPatchGroup::~FPCGExGraphPatchGroup()
{
	for (const FPCGExGraphPatch* Patch : Patches) { delete Patch; }

	Patches.Empty();
	IndicesMap.Empty();
	PointIO = nullptr;
	CurrentGraph = nullptr;
	PatchesIO = nullptr;
}

void FPCGExGraphPatchGroup::OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount, const uint32 PUID)
{
	PatchesIO = new FPCGExPointIOGroup();
	int32 PatchIndex = 0;
	for (FPCGExGraphPatch* Patch : Patches)
	{
		const int64 OutNumPoints = Patch->IndicesSet.Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { continue; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { continue; }
		FPCGExPointIO& OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExPointIO::EInit::NewOutput);
		OutIO.GetOut()->Metadata->CreateAttribute<int32>(PCGExGraph::PUIDAttributeName, PUID, false, true);
		Patch->OutputTo(OutIO, PatchIndex);
		PatchIndex++;
	}
	PatchesIO->OutputTo(Context);
	delete PatchesIO;
}

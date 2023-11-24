// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraphPatch.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExGraphParamsData.h"

#include "Graph/PCGExGraph.h"

void UPCGExGraphPatch::Add(const uint64 Hash)
{
	{
		FWriteScopeLock ScopeLock(HashLock);
		HashSet.Add(Hash);
	}
	{
		FWriteScopeLock ScopeLock(Parent->HashLock);
		Parent->HashMap.Add(Hash, this);
	}
}

bool UPCGExGraphPatch::Contains(const uint64 Hash) const
{
	FReadScopeLock ScopeLock(HashLock);
	return HashSet.Contains(Hash);
}

bool UPCGExGraphPatch::OutputTo(const UPCGExPointIO* OutIO, int32 PatchIDOverride)
{
	const TArray<FPCGPoint>& InPoints = OutIO->In->GetPoints();
	TArray<FPCGPoint>& Points = OutIO->Out->GetMutablePoints();
	Points.Reserve(Points.Num() + HashSet.Num());
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchIDAttributeName, PatchIDOverride < 0 ? PatchID : PatchIDOverride);
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchSizeAttributeName, HashSet.Num());
	for (const int32 Index : HashSet)
	{
		FPCGPoint& NewPoint = Points.Emplace_GetRef(InPoints[Index]);
	}
	return true;
}

void UPCGExGraphPatch::Flush()
{
	HashSet.Empty();
	PointIO = nullptr;
	Parent = nullptr;
}

bool UPCGExGraphPatchGroup::Contains(const uint64 Hash) const
{
	FReadScopeLock ScopeLock(HashLock);
	return HashMap.Contains(Hash);
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::FindPatch(uint64 Hash)
{
	FReadScopeLock ScopeLock(HashLock);

	UPCGExGraphPatch** PatchPtr = HashMap.Find(Hash);
	if (!PatchPtr) { return nullptr; }

	return *PatchPtr;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::GetOrCreatePatch(const uint64 Hash)
{
	{
		FReadScopeLock ScopeLock(HashLock);
		if (UPCGExGraphPatch** PatchPtr = HashMap.Find(Hash)) { return *PatchPtr; }
	}

	UPCGExGraphPatch* NewPatch = CreatePatch();
	NewPatch->Add(Hash);
	return NewPatch;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::CreatePatch()
{
	FWriteScopeLock ScopeLock(PatchesLock);
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
	Graph->GetEdges(InIndex, PointIO->In->GetPoint(InIndex).MetadataEntry, UnsignedEdges);

	for (const PCGExGraph::FUnsignedEdge& Edge : UnsignedEdges)
	{
		if (static_cast<uint8>((Edge.Type & static_cast<EPCGExEdgeType>(CrawlEdgeTypes))) == 0) { continue; }

		if (!Patch)
		{
			Patch = GetOrCreatePatch(InIndex); // Overlap

			//TODO: Support resolve method
			switch (ResolveRoamingMethod)
			{
			default:
			case EPCGExRoamingResolveMethod::Overlap:
				// Create patch from initial point.
				break;
			case EPCGExRoamingResolveMethod::Merge:
				// Check if this is a roaming
				break;
			case EPCGExRoamingResolveMethod::Cutoff:
				break;
			}
		}

		Distribute(Edge.End, Patch);
	}
}

template <typename T>
void UPCGExGraphPatchGroup::DistributeEdge(const T& InEdge, UPCGExGraphPatch* Patch)
{
	if (Patch)
	{
		if (Patch->Contains(InEdge))
		{
			// Exit early since this edge has already been registered in this patch.
			return;
		}

		// Otherwise add edge to active patch
		Patch->Add(InEdge);
	}

	TArray<T> Edges;
	Graph->GetEdges(InEdge.Start, PointIO->In->GetPoint(InEdge.Start).MetadataEntry, Edges);
	Graph->GetEdges(InEdge.End, PointIO->In->GetPoint(InEdge.End).MetadataEntry, Edges);

	for (const T& Edge : Edges)
	{
		if (static_cast<uint8>((Edge.Type & static_cast<EPCGExEdgeType>(CrawlEdgeTypes))) == 0) { continue; }

		if (!Patch)
		{
			Patch = GetOrCreatePatch(InEdge); // Overlap

			//TODO: Support resolve method
			switch (ResolveRoamingMethod)
			{
			default:
			case EPCGExRoamingResolveMethod::Overlap:
				// Create patch from initial point.
				break;
			case EPCGExRoamingResolveMethod::Merge:
				// Check if this is a roaming
				break;
			case EPCGExRoamingResolveMethod::Cutoff:
				break;
			}
		}

		DistributeEdge(Edge, Patch);
	}
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExIO::EInitMode::NewOutput);
		Patch->OutputTo(OutIO, -1);
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

void UPCGExGraphPatchGroup::Flush()
{
	for (UPCGExGraphPatch* Patch : Patches) { Patch->Flush(); }
	Patches.Empty();
	HashMap.Empty();
	PointIO = nullptr;
	Graph = nullptr;
	PatchesIO = nullptr;
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	int32 PatchIndex = 0;
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const int64 OutNumPoints = Patch->HashSet.Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { continue; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { continue; }
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExIO::EInitMode::NewOutput);
		Patch->OutputTo(OutIO, PatchIndex);
		PatchIndex++;
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

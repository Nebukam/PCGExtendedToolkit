// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraphPatch.h"
#include "PCGContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

void UPCGExGraphPatch::Add(int32 Index)
{
	{
		FWriteScopeLock ScopeLock(IndicesLock);
		Indices.Add(Index);
	}
	{
		FWriteScopeLock ScopeLock(Parent->MapLock);
		Parent->PatchMap.Add(Index, this);
	}
}

bool UPCGExGraphPatch::Contains(const int32 Index) const
{
	FReadScopeLock ScopeLock(IndicesLock);
	return Indices.Contains(Index);
}

bool UPCGExGraphPatch::OutputTo(const UPCGExPointIO* OutIO, int32 PatchIDOverride)
{
	const TArray<FPCGPoint>& InPoints = OutIO->In->GetPoints();
	TArray<FPCGPoint>& Points = OutIO->Out->GetMutablePoints();
	Points.Reserve(Points.Num() + Indices.Num());
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchIDAttributeName, PatchIDOverride < 0 ? PatchID : PatchIDOverride);
	PCGMetadataElementCommon::ClearOrCreateAttribute(OutIO->Out->Metadata, Parent->PatchSizeAttributeName, Indices.Num());
	for (const int32 Index : Indices)
	{
		FPCGPoint& NewPoint = Points.Emplace_GetRef(InPoints[Index]);
	}
	return true;
}

void UPCGExGraphPatch::Flush()
{
	Indices.Empty();
	IO = nullptr;
	Parent = nullptr;
}

bool UPCGExGraphPatchGroup::Contains(const int32 Index) const
{
	FReadScopeLock ScopeLock(MapLock);
	return PatchMap.Contains(Index);
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::FindPatch(int32 Index)
{
	UPCGExGraphPatch** PatchPtr = PatchMap.Find(Index);
	if (!PatchPtr) { return nullptr; }
	return *PatchPtr;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::GetOrCreatePatch(int32 Index)
{
	{
		FReadScopeLock ScopeLock(MapLock);
		if (UPCGExGraphPatch** PatchPtr = PatchMap.Find(Index)) { return *PatchPtr; }
	}

	UPCGExGraphPatch* NewPatch = CreatePatch();
	NewPatch->Add(Index);
	return NewPatch;
}

UPCGExGraphPatch* UPCGExGraphPatchGroup::CreatePatch()
{
	FWriteScopeLock ScopeLock(PatchesLock);
	UPCGExGraphPatch* NewPatch = NewObject<UPCGExGraphPatch>();
	Patches.Add(NewPatch);
	NewPatch->Parent = this;
	NewPatch->IO = IO;
	NewPatch->PatchID = Patches.Num() - 1;
	return NewPatch;
}

void UPCGExGraphPatchGroup::Distribute(const FPCGPoint& Point, const int32 ReadIndex, UPCGExGraphPatch* Patch)
{
	if (Patch)
	{
		if (Patch->Contains(ReadIndex))
		{
			// Exit early since this point index has already been registered in this patch.
			return;
		}

		// Add index to active patch, if it exists
		Patch->Add(ReadIndex);
	}

	TArray<PCGExGraph::FSocketMetadata> SocketMetadatas;
	Graph->GetSocketsData(Point.MetadataEntry, SocketMetadatas);

	for (const PCGExGraph::FSocketMetadata& Data : SocketMetadatas)
	{
		if (Data.Index == -1) { continue; }
		if (static_cast<uint8>((Data.EdgeType & static_cast<EPCGExEdgeType>(CrawlEdgeTypes))) == 0) { continue; }

		const int32 PtIndex = Data.Index;
		if (!Patch) { Patch = GetOrCreatePatch(ReadIndex); } // Point is not roaming, create a patch so we can add the next neighbor

		Distribute(IO->In->GetPoint(Data.Index), PtIndex, Patch);
	}
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*IO, PCGEx::EIOInit::NewOutput);
		Patch->OutputTo(OutIO, -1);
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

void UPCGExGraphPatchGroup::Flush()
{
	for (UPCGExGraphPatch* Patch : Patches){Patch->Flush();}
	Patches.Empty();
	PatchMap.Empty();
	IO = nullptr;
	Graph = nullptr;
	PatchesIO = nullptr;
}

void UPCGExGraphPatchGroup::OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount)
{
	PatchesIO = NewObject<UPCGExPointIOGroup>();
	int32 PatchIndex = 0;
	for (UPCGExGraphPatch* Patch : Patches)
	{
		const int64 OutNumPoints = Patch->Indices.Num();
		if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { continue; }
		if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { continue; }
		const UPCGExPointIO* OutIO = PatchesIO->Emplace_GetRef(*IO, PCGEx::EIOInit::NewOutput);
		Patch->OutputTo(OutIO, PatchIndex);
		PatchIndex++;
	}
	PatchesIO->OutputTo(Context);
	PatchesIO->Flush();
	Flush();
}

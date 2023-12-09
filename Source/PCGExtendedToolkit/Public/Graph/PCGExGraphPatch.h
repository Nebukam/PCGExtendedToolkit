// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExGraphParamsData.h"
#include "..\Data\PCGExPointsIO.h"
#include "UObject/Object.h"

#include "PCGExGraphPatch.generated.h"

class FPCGExGraphPatchGroup;

UENUM(BlueprintType)
enum class EPCGExRoamingResolveMethod : uint8
{
	Overlap UMETA(DisplayName = "Overlap", ToolTip="Roaming nodes with unidirectional connections will create their own overlapping patches."),
	Merge UMETA(DisplayName = "Merge", ToolTip="Roaming patches will be merged into existing ones; thus creating less patches yet not canon ones."),
	Cutoff UMETA(DisplayName = "Cutoff", ToolTip="Roaming patches discovery will be cut off where they would otherwise overlap."),
};

/**
 * 
 */
class PCGEXTENDEDTOOLKIT_API FPCGExGraphPatch
{
public:
	~FPCGExGraphPatch();

	FPCGExPointIO* PointIO = nullptr;
	FPCGExGraphPatchGroup* Parent = nullptr;

	int32 PatchID = -1;

	TSet<int32> IndicesSet;
	TSet<uint64> EdgesHashSet;
	mutable FRWLock HashLock;

	void Add(int32 InIndex);
	bool Contains(const int32 InIndex) const;

	void AddEdge(uint64 InEdgeHash);
	bool ContainsEdge(const uint64 InEdgeHash) const;

	bool OutputTo(const FPCGExPointIO& OutIO, int32 PatchIDOverride);
};

class PCGEXTENDEDTOOLKIT_API FPCGExGraphPatchGroup
{
public:
	~FPCGExGraphPatchGroup();

	TArray<FPCGExGraphPatch*> Patches;
	mutable FRWLock PatchesLock;
	int32 NumMaxEdges = 8;

	TMap<uint64, FPCGExGraphPatch*> IndicesMap;
	mutable FRWLock HashLock;
	EPCGExEdgeType CrawlEdgeTypes;

	UPCGExGraphParamsData* CurrentGraph = nullptr;
	FPCGExPointIO* PointIO = nullptr;
	FPCGExPointIOGroup* PatchesIO = nullptr;

	FName PatchIDAttributeName;
	FName PatchSizeAttributeName;

	bool Contains(const uint64 Hash) const;

	FPCGExGraphPatch* FindPatch(uint64 Hash);
	FPCGExGraphPatch* GetOrCreatePatch(uint64 Hash);
	FPCGExGraphPatch* CreatePatch();

	void Distribute(const int32 InIndex, FPCGExGraphPatch* Patch = nullptr);

	void OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount, const uint32 PUID);
	void OutputTo(FPCGContext* Context);
};

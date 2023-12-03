// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExGraphParamsData.h"
#include "Data/PCGExPointIO.h"
#include "UObject/Object.h"

#include "PCGExGraphPatch.generated.h"

class UPCGExGraphPatchGroup;

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
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExGraphPatch : public UObject
{
	GENERATED_BODY()

public:
	//UPCGExGraphPatch();

	UPCGExPointIO* PointIO = nullptr;
	UPCGExGraphPatchGroup* Parent = nullptr;

	int32 PatchID = -1;

	TSet<uint64> HashSet;
	mutable FRWLock HashLock;

	void Add(uint64 Hash);
	bool Contains(const uint64 Hash) const;

	bool OutputTo(const UPCGExPointIO* OutIO, int32 PatchIDOverride);

	void Flush();

public:
	virtual ~UPCGExGraphPatch() override
	{
		PointIO = nullptr;
		Parent = nullptr;
	}
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExGraphPatchGroup : public UObject
{
	GENERATED_BODY()

public:
	TArray<UPCGExGraphPatch*> Patches;
	mutable FRWLock PatchesLock;
	int32 NumMaxEdges = 8;

	TMap<uint64, UPCGExGraphPatch*> HashMap;
	mutable FRWLock HashLock;
	EPCGExEdgeType CrawlEdgeTypes;

	UPCGExGraphParamsData* Graph = nullptr;
	UPCGExPointIO* PointIO = nullptr;
	UPCGExPointIOGroup* PatchesIO = nullptr;

	FName PatchIDAttributeName;
	FName PatchSizeAttributeName;

	bool Contains(const uint64 Hash) const;

	UPCGExGraphPatch* FindPatch(uint64 Hash);
	UPCGExGraphPatch* GetOrCreatePatch(uint64 Hash);
	UPCGExGraphPatch* CreatePatch();

	void Distribute(const int32 InIndex, UPCGExGraphPatch* Patch = nullptr);
	template <typename T>
	void DistributeEdge(const T& InEdge, UPCGExGraphPatch* Patch = nullptr);

	void OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount);
	void OutputTo(FPCGContext* Context);

	void Flush();

public:
	virtual ~UPCGExGraphPatchGroup() override
	{
		PointIO = nullptr;
		Graph = nullptr;
		PatchesIO = nullptr;
	}
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "Data/PCGExGraphParamsData.h"
#include "UObject/Object.h"
#include "PCGExGraphPatch.generated.h"

class UPCGExGraphPatchGroup;

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExGraphPatch : public UObject
{
	GENERATED_BODY()

public:
	//UPCGExGraphPatch();

	UPCGExPointIO* IO = nullptr;
	UPCGExGraphPatchGroup* Parent = nullptr;

	int64 PatchID = -1;

	TSet<int32> Indices;
	mutable FRWLock IndicesLock;

	void Add(int32 Index);
	bool Contains(const int32 Index) const;

	bool OutputTo(const UPCGExPointIO* OutIO, int32 PatchIDOverride);

	void Flush();
	
public:
	~UPCGExGraphPatch()
	{
		IO = nullptr;
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

	TMap<int32, UPCGExGraphPatch*> PatchMap;
	mutable FRWLock MapLock;
	EPCGExEdgeType CrawlEdgeTypes;

	UPCGExGraphParamsData* Graph = nullptr;
	UPCGExPointIO* IO = nullptr;
	UPCGExPointIOGroup* PatchesIO = nullptr;

	FName PatchIDAttributeName;
	FName PatchSizeAttributeName;
	
	bool Contains(const int32 Index) const;

	UPCGExGraphPatch* FindPatch(int32 Index);
	UPCGExGraphPatch* GetOrCreatePatch(int32 Index);
	UPCGExGraphPatch* CreatePatch();
	void Distribute(const FPCGPoint& Point, const int32 ReadIndex, UPCGExGraphPatch* Patch = nullptr);

	void OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount);
	void OutputTo(FPCGContext* Context);
	
	void Flush();

public:
	~UPCGExGraphPatchGroup()
	{
		IO = nullptr;
		Graph = nullptr;
		PatchesIO = nullptr;
	}
};

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"

#include "PCGExFindEdgePatches.generated.h"

UENUM(BlueprintType)
enum class EPCGExRoamingResolveMethod : uint8
{
	Overlap UMETA(DisplayName = "Overlap", ToolTip="Roaming nodes with unidirectional connections will create their own overlapping patches."),
	Merge UMETA(DisplayName = "Merge", ToolTip="Roaming patches will be merged into existing ones; thus creating less patches yet not canon ones."),
	Cutoff UMETA(DisplayName = "Cutoff", ToolTip="Roaming patches discovery will be cut off where they would otherwise overlap."),
};

namespace PCGExGraph
{
	class FPatchGroup;

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPatch
	{
	public:
		~FPatch();

		PCGExData::FPointIO* PointIO = nullptr;
		FPatchGroup* Parent = nullptr;

		int32 PatchID = -1;

		TSet<int32> IndicesSet;
		TSet<uint64> EdgesHashSet;
		mutable FRWLock HashLock;

		void Add(int32 InIndex);
		bool Contains(const int32 InIndex) const;

		void AddEdge(uint64 InEdgeHash);
		bool ContainsEdge(const uint64 InEdgeHash) const;

		bool OutputTo(const PCGExData::FPointIO& OutIO, int32 PatchIDOverride);
	};

	class PCGEXTENDEDTOOLKIT_API FPatchGroup
	{
	public:
		~FPatchGroup();

		TArray<FPatch*> Patches;
		mutable FRWLock PatchesLock;
		int32 NumMaxEdges = 8;

		TMap<uint64, FPatch*> IndicesMap;
		mutable FRWLock HashLock;
		EPCGExEdgeType CrawlEdgeTypes;

		UPCGExGraphParamsData* CurrentGraph = nullptr;
		PCGExData::FPointIO* PointIO = nullptr;
		PCGExData::FPointIOGroup* PatchesIO = nullptr;

		FName PatchIDAttributeName;
		FName PatchSizeAttributeName;

		bool Contains(const uint64 Hash) const;

		FPatch* FindPatch(uint64 Hash);
		FPatch* GetOrCreatePatch(uint64 Hash);
		FPatch* CreatePatch();

		FPatch* Distribute(const int32 InIndex, FPatch* Patch = nullptr);

		void OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount, const uint32 PUID);
		void OutputTo(FPCGContext* Context);
	};
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgePatchesSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgePatches, "Find Edge Patches", "Create partitions from interconnected points. Each patch is the result of all input graphs combined.");
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;

public:
	/** Edge types to crawl to create a patch */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Don't output patches if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallPatches = true;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallPatches"))
	int32 MinPatchSize = 3;

	/** Don't output patches if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigPatches = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigPatches"))
	int32 MaxPatchSize = 500;

	/** Name of the attribute to ouput the unique patch identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName PatchIDAttributeName = "PatchID";

	/** Name of the attribute to output the patch size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName PatchSizeAttributeName = "PatchSize";

	/** Not implemented yet, always Overlap */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRoamingResolveMethod ResolveRoamingMethod = EPCGExRoamingResolveMethod::Overlap;

private:
	friend class FPCGExFindEdgePatchesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgePatchesContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExFindEdgePatchesElement;

	virtual ~FPCGExFindEdgePatchesContext() override;

	EPCGExEdgeType CrawlEdgeTypes;
	bool bRemoveSmallPatches;
	int32 MinPatchSize;
	bool bRemoveBigPatches;
	int32 MaxPatchSize;

	PCGExData::FPointIOGroup* PatchesIO;
	int32 PatchUIndex = 0;

	FName PatchIDAttributeName;
	FName PatchSizeAttributeName;
	FName PointUIDAttributeName;

	mutable FRWLock EdgesHashLock; 
	TSet<uint64> EdgesHash;
	
	PCGExGraph::FPatchGroup* Patches;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;
	
	FPCGMetadataAttribute<int64>* InCachedIndex;

	void PreparePatchGroup()
	{
		Patches = new PCGExGraph::FPatchGroup();
		Patches->CrawlEdgeTypes = CrawlEdgeTypes;
		Patches->PatchIDAttributeName = PatchIDAttributeName;
		Patches->PatchIDAttributeName = PatchIDAttributeName;
	}

	void UpdatePatchGroup() const
	{
		Patches->NumMaxEdges = CurrentGraph->GetSocketMapping()->Sockets.Num();
		Patches->CurrentGraph = CurrentGraph;
		Patches->PointIO = CurrentIO;
	}
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgePatchesElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FDistributeToPatchTask : public FPCGExNonAbandonableTask
{
public:
	FDistributeToPatchTask(FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FConsolidatePatchesTask : public FPCGExNonAbandonableTask
{
public:
	FConsolidatePatchesTask(FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FWritePatchesTask : public FPCGExNonAbandonableTask
{
public:
	FWritePatchesTask(FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO,
	                  PCGExGraph::FPatch* InPatch, UPCGPointData* InPatchData) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO),
		Patch(InPatch), PatchData(InPatchData)
	{
	}

	PCGExGraph::FPatch* Patch;
	UPCGPointData* PatchData;

	virtual bool ExecuteTask() override;
};

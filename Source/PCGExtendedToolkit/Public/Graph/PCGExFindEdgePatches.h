// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "PCGExGraphPatch.h"

#include "PCGExFindEdgePatches.generated.h"

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

	virtual PCGExData::EInit GetPointOutputInitMode() const override;

public:
	/** Edge types to crawl to create a patch */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallPatches = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallPatches"))
	int32 MinPatchSize = 3;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigPatches = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigPatches"))
	int32 MaxPatchSize = 500;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName PatchIDAttributeName = "PatchID";

	/** TBD */
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

	FPCGExGraphPatchGroup* Patches;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;

	FPCGMetadataAttribute<int64>* InCachedIndex;

	void PreparePatchGroup()
	{
		Patches = new FPCGExGraphPatchGroup();
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
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	virtual bool Validate(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FDistributeToPatchTask : public FPCGExAsyncTask
{
public:
	FDistributeToPatchTask(UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO) :
		FPCGExAsyncTask(InManager, InInfos, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FConsolidatePatchesTask : public FPCGExAsyncTask
{
public:
	FConsolidatePatchesTask(UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO) :
		FPCGExAsyncTask(InManager, InInfos, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FWritePatchesTask : public FPCGExAsyncTask
{
public:
	FWritePatchesTask(UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO,
	                  FPCGExGraphPatch* InPatch, UPCGPointData* InPatchData) :
		FPCGExAsyncTask(InManager, InInfos, InPointIO),
		Patch(InPatch), PatchData(InPatchData)
	{
	}

	FPCGExGraphPatch* Patch;
	UPCGPointData* PatchData;

	virtual bool ExecuteTask() override;
};

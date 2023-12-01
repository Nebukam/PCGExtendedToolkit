// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "PCGExGraphPatch.h"

#include "PCGExPartitionGraphPatches.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPartitionGraphPatchesSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionGraphPatches, "Partition Graph Patches", "Create partitions from interconnected points");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

public:
	/** Edge types to crawl to create a patch */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallPatches = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallPatches"))
	int64 MinPatchSize = 3;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigPatches = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigPatches"))
	int64 MaxPatchSize = 500;

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
	friend class FPCGExPartitionGraphPatchesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPartitionGraphPatchesContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExPartitionGraphPatchesElement;

public:
	EPCGExEdgeType CrawlEdgeTypes;
	bool bRemoveSmallPatches;
	int64 MinPatchSize;
	bool bRemoveBigPatches;
	int64 MaxPatchSize;

	FName PatchIDAttributeName;
	FName PatchSizeAttributeName;

	UPCGExGraphPatchGroup* Patches;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;

	FPCGMetadataAttribute<int64>* InCachedIndex;

	void PreparePatchGroup()
	{
		Patches = NewObject<UPCGExGraphPatchGroup>();
		Patches->Graph = CurrentGraph;
		Patches->PointIO = CurrentIO;
		Patches->CrawlEdgeTypes = CrawlEdgeTypes;
		Patches->ResolveRoamingMethod = ResolveRoamingMethod;
		Patches->PatchIDAttributeName = PatchIDAttributeName;
		Patches->PatchIDAttributeName = PatchIDAttributeName;
	}
};


class PCGEXTENDEDTOOLKIT_API FPCGExPartitionGraphPatchesElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"

#include "PCGExConsolidateGraph.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExConsolidateGraphSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConsolidateGraph, "Graph : Consolidate", "Repairs and consolidate graph indices after points have been removed post graph-building.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

	/** Compute edge types internally. If you don't need edge types, set it to false to save some cycles.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bConsolidateEdgeType = true;

private:
	friend class FPCGExConsolidateGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConsolidateGraphContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExConsolidateGraphElement;

public:
	bool bConsolidateEdgeType;

	TMap<int64, int64> IndicesRemap;
	mutable FRWLock IndicesLock;
};


class PCGEXTENDEDTOOLKIT_API FPCGExConsolidateGraphElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
#if WITH_EDITOR
	static int64 GetFixedIndex(FPCGExConsolidateGraphContext* Context, int64 InIndex);
#endif
};

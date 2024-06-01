// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"

#include "PCGExSanitizeCustomGraph.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExConsolidateCustomGraphSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SanitizeCustomGraph, "Custom Graph : Sanitize", "Repairs and consolidate graph indices after points have been removed post graph-building.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

private:
	friend class FPCGExConsolidateCustomGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConsolidateCustomGraphContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExConsolidateCustomGraphElement;

public:
	TMap<int64, int64> IndicesRemap;
	mutable FRWLock IndicesLock;
};


class PCGEXTENDEDTOOLKIT_API FPCGExConsolidateCustomGraphElement : public FPCGExCustomGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	static int64 GetFixedIndex(FPCGExConsolidateCustomGraphContext* Context, int64 InIndex);
};

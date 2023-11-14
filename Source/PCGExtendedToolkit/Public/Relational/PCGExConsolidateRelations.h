// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExConsolidateRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational")
class PCGEXTENDEDTOOLKIT_API UPCGExConsolidateRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConsolidateRelations, "Consolidate Relations", "Repairs and consolidate relations indices after points have been removed post relations-building.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

private:
	friend class FPCGExConsolidateRelationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConsolidateRelationsContext : public FPCGExRelationsProcessorContext
{
	friend class FPCGExConsolidateRelationsElement;

public:
	TMap<int64, int64> Deltas;
	mutable FRWLock DeltaLock;
};


class PCGEXTENDEDTOOLKIT_API FPCGExConsolidateRelationsElement : public FPCGExRelationsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
#if WITH_EDITOR
	static int64 GetFixedIndex(FPCGExConsolidateRelationsContext* Context, int64 InIndex);
#endif
};

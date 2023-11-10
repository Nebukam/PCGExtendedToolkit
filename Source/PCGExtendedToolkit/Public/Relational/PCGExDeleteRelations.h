// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExDeleteRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExDeleteRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DeleteRelations")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExDeleteRelations", "NodeTitle", "Delete Relations"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

public:


private:
	friend class FPCGExDeleteRelationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDeleteRelationsContext : public FPCGExRelationsProcessorContext
{
	friend class FPCGExDeleteRelationsElement;
};


class PCGEXTENDEDTOOLKIT_API FPCGExDeleteRelationsElement : public FPCGExRelationsProcessorElement
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
};

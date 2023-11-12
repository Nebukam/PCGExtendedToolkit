// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExWriteIndex.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExWriteIndexSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("WriteIndex")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExWriteIndex", "NodeTitle", "Write Index"); }
	virtual FText GetNodeTooltipText() const override;
#endif

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutputAttributeName;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSortPointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;

public:
	mutable FRWLock MapLock;
	FName OutName = NAME_None;
	TMap<UPCGExPointIO*, FPCGMetadataAttribute<int64>*> AttributeMap;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteIndexElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

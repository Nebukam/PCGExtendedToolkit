// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsParamsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExFindRelations.generated.h"

UENUM()
enum EFindRelationsOperationState : int
{
	Setup = -1 UMETA(DisplayName = "Setup"),
	ReadyForNextPoints = 0 UMETA(DisplayName = "Ready for next points"),
	ReadyForNextParams = 1 UMETA(DisplayName = "Ready for next params"),
	ProcessingParams = 2 UMETA(DisplayName = "Processing params"),
	Done = 77 UMETA(DisplayName = "Done")
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExFindRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FindRelations")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExFindRelations", "NodeTitle", "Find Relations"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:


private:
	friend class FPCGExFindRelationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindRelationsContext : public FPCGExRelationsProcessorContext
{
	friend class FPCGExFindRelationsElement;

public:
	UPCGPointData::PointOctree* Octree = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindRelationsElement : public FPCGExRelationsProcessorElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual void DrawRelationsDebug(FPCGExFindRelationsContext* Context) const;
};

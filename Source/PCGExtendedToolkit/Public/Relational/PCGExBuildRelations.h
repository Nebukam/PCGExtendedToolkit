// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExBuildRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExBuildRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("BuildRelations")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExBuildRelations", "NodeTitle", "Build Relations"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;
	
	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

public:


private:
	friend class FPCGExBuildRelationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildRelationsContext : public FPCGExRelationsProcessorContext
{
	friend class FPCGExBuildRelationsElement;

public:
	UPCGPointData::PointOctree* Octree = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildRelationsElement : public FPCGExRelationsProcessorElement
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
	virtual void DrawRelationsDebug(FPCGExBuildRelationsContext* Context) const;
#endif
	
};

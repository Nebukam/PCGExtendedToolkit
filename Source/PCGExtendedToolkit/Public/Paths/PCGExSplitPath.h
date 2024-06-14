// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExSplitPath.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action"))
enum class EPCGExPathSplitAction : uint8
{
	Split UMETA(DisplayName = "Split", ToolTip="Duplicate the split point so the original becomes a new end, and the copy a new start."),
	Remove UMETA(DisplayName = "Remove", ToolTip="Remove the split point, shrinking both the previous and next paths."),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSplitPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SplitPath, "Path : Points Intersection", "Find intersection with target input points.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual FName GetPointFilterLabel() const override;
	virtual bool RequiresPointFilters() const override;

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExPathSplitAction SplitAction = EPCGExPathSplitAction::Split;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSplitPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExSplitPathElement;

	virtual ~FPCGExSplitPathContext() override;

	virtual bool PrepareFiltersWithAdvance() const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSplitPathElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSplitPathTask final : public FPCGExNonAbandonableTask
{
public:
	FPCGExSplitPathTask(PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathProcessor.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Subdivide Mode"))
enum class EPCGExSubdivideMode : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is static"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPath; }
#endif
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
};

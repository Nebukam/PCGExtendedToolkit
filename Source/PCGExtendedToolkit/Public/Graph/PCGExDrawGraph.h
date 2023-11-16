// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraphProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExDrawGraph.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawGraphSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDrawGraphSettings(const FObjectInitializer& ObjectInitializer);
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawGraph, "Draw Graph", "Draw graph edges. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(1.0f,0.0f,0.0f, 1.0f); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Draw edges.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawGraph = true;

	/** Draw only one type of edge.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bFilterEdges = false;

	/** Type of edge to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bFilterEdges"))
	EPCGExEdgeType EdgeType = EPCGExEdgeType::Unknown;
	
	/** Draw socket cones lines.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawSocketCones = false;
	
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

	

private:
	friend class FPCGExDrawGraphElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDrawGraphElement : public FPCGExGraphProcessorElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

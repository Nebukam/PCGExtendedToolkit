// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"

#include "PCGExDrawCustomGraph.generated.h"

class UPCGExCustomGraphSolver;
/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawCustomGraphSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDrawCustomGraphSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawCustomGraph, "Draw Custom Graph", "Draw graph edges. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorDebug; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Draw edges.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawCustomGraph = true;

	/** Type of edge to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeCrawlingSettings EdgeCrawlingSettings;

	/** Draw socket cones lines.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawSocketCones = false;

	/** Draw socket loose bounds.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawSocketBox = false;

	/** Debug drawing toggle. Exposed to have more control on debug draw in sub-graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug", meta=(PCG_Overridable))
	bool bPCGExDebug = true;

private:
	friend class FPCGExDrawCustomGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDrawCustomGraphContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExBuildCustomGraphElement;
	friend class FPCGExProbeTask;

public:
	UPCGExCustomGraphSolver* GraphSolver = nullptr;
	bool bMoveSocketOriginOnPointExtent = false;

	UPCGPointData::PointOctree* Octree = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDrawCustomGraphElement : public FPCGExCustomGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

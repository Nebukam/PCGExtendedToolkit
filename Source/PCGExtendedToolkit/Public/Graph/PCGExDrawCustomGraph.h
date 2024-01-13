// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"

#include "PCGExCustomGraphProcessor.h"

#include "PCGExDrawCustomGraph.generated.h"

class UPCGExCustomGraphSolver;
/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawCustomGraphSettings : public UPCGExCustomGraphProcessorSettings, public IPCGExDebug
{
	GENERATED_BODY()

public:
	UPCGExDrawCustomGraphSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawCustomGraph, "Draw Graph", "Draw graph edges. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorDebug; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

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

	//~Begin IPCGExDebug interface
public:
#if WITH_EDITOR
	virtual bool IsDebugEnabled() const override { return bEnabled && bDebug; }
#endif

	//~End IPCGExDebug interface

public:
	/** Draw edges.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawCustomGraph = true;

	/** Type of edge to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 EdgeType = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Draw socket cones lines.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawSocketCones = false;

	/** Draw socket loose bounds.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDrawSocketBox = false;

private:
	friend class FPCGExDrawCustomGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDrawCustomGraphContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExBuildCustomGraphElement;
	friend class FProbeTask;

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

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

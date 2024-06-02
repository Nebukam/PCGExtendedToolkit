// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCustomGraphProcessor.h"
#include "Graph/Edges/Promoting/PCGExEdgePromotingOperation.h"
#include "PCGExPromoteEdges.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPromoteEdgesSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PromoteEdges, "Custom Graph : Promote Edges", "Promote edges to points or small paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorPathfinding; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
public:
	virtual void PostInitProperties() override;

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface


public:
	/** Type of edge to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeCrawlingSettings EdgeTypesSettings;

	/**  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExEdgePromotingOperation> Promotion;

private:
	friend class FPCGExPromoteEdgesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPromoteEdgesContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExPromoteEdgesElement;

public:
	int32 MaxPossibleEdgesPerPoint = 0;
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	mutable FRWLock EdgeLock;

	UPCGExEdgePromotingOperation* Promotion;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPromoteEdgesElement : public FPCGExCustomGraphProcessorElement
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
